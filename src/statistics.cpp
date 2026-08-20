/*
  Color Bandwidth Meter (CBM) - display in real time the network traffic speed

  Copyright 2005-2006 Aaron Isotton <aaron@isotton.com>
  Copyright 2024      David Polverari <david.polverari@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; version 2
  of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "config.h"
#include "statistics.hpp"
#include "ErrnoError.hpp"
#include <algorithm>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sstream>

#ifdef __APPLE__
#include <net/if.h>
#include <net/route.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#endif

#define PROC_NET_DEV "/proc/net/dev"

namespace statistics {

Interface::Interface(const std::string& name)
    : name_(name), updated_(false), initialized_(false), warmupCount_(0),
      receiveSpeed_(0.0), transmitSpeed_(0.0),
      receiveMax_(0.0), transmitMax_(0.0) {
    memset(statistics_, 0, sizeof(statistics_));
}

class InterfaceNameMatchesPredicate {
public:
    InterfaceNameMatchesPredicate(const std::string& name) : name_(name) {}

    bool operator() (const Interface& interface) const {
        return name_ == interface.getName();
    }
private:
    std::string name_;
};

class InterfaceNotUpdatedPredicate {
public:
    bool operator() (const Interface& interface) const {
        return !interface.getUpdated();
    }
};

void Interface::update(const Statistics& statistics) {
    updated_ = true;

    // First sample: there is no previous sample to diff against, so
    // speeds cannot be computed yet. Keep them at zero.
    if (!initialized_) {
        memcpy(statistics_, &statistics, sizeof(Statistics));
        initialized_ = true;
        return;
    }

    memcpy(statistics_ + 1, statistics_ + 0, sizeof(Statistics));
    memcpy(statistics_, &statistics, sizeof(Statistics));

    const Statistics& x0 = statistics_[1];
    const Statistics& x1 = statistics_[0];

    double timeDelta =
        (x1.timestamp.tv_sec - x0.timestamp.tv_sec) * 1.
        + (x1.timestamp.tv_usec - x0.timestamp.tv_usec) * .000001;

    const bool countersReset =
        x1.rx_bytes < x0.rx_bytes || x1.tx_bytes < x0.tx_bytes;

    if (timeDelta <= 0. || countersReset) {
        // Counter reset/wrap or non-positive dt: delta is meaningless. Report
        // zero, keep the new sample as baseline (memcpy above already stored
        // it), leave the maxima untouched.
        receiveSpeed_ = 0.0;
        transmitSpeed_ = 0.0;
        warmupCount_++;
        return;
    }

    receiveSpeed_ = (x1.rx_bytes - x0.rx_bytes) / timeDelta;
    transmitSpeed_ = (x1.tx_bytes - x0.tx_bytes) / timeDelta;

    warmupCount_++;

    // Waits some iterations before calculating max speeds.
    // This avoids presenting wrong initial peaks.
    if (warmupCount_ < 8)
        return;

    if (receiveSpeed_ > receiveMax_)
        receiveMax_ = receiveSpeed_;
    if (transmitSpeed_ > transmitMax_)
        transmitMax_ = transmitSpeed_;
}

SampleList parseProcNetDev(const std::string& content,
                           const struct timeval& timestamp) {
    SampleList samples;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {      // replaces while(!feof)+unchecked fgets
        std::string::size_type colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string name = line.substr(0, colon);
        std::string::size_type start = name.find_first_not_of(" \t");
        name = (start == std::string::npos) ? std::string() : name.substr(start);

        Statistics stats;
        std::memset(&stats, 0, sizeof(stats));
        stats.timestamp = timestamp;
        if (sscanf(line.c_str() + colon + 1,
                   "%llu %llu %llu %llu %llu %llu %llu %llu "
                   "%llu %llu %llu %llu %llu %llu %llu %llu",
                   &stats.rx_bytes, &stats.rx_packets, &stats.rx_errs, &stats.rx_drop,
                   &stats.rx_fifo, &stats.rx_frame, &stats.rx_compressed, &stats.rx_multicast,
                   &stats.tx_bytes, &stats.tx_packets, &stats.tx_errs, &stats.tx_drop,
                   &stats.tx_fifo, &stats.tx_frame, &stats.tx_compressed, &stats.tx_multicast)
                == 16) {
            Sample sample = { name, stats };
            samples.push_back(sample);
        }
    }
    return samples;
}

#ifdef __APPLE__
// Read per-interface statistics through the NET_RT_IFLIST2 sysctl,
// which reports one if_msghdr2 per interface. All samples share
// `timestamp`, mirroring the /proc/net/dev reader. Two kernel quirks
// affect non-platform binaries: byte counters advance in 1 KiB steps,
// and they wrap at 4 GiB; the counter-reset handling in
// Interface::update copes with the wrap.
static SampleList readInterfaceStatistics(const struct timeval& timestamp) {
    SampleList samples;

    int mib[6] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0 };

    size_t size = 0;
    if (sysctl(mib, 6, NULL, &size, NULL, 0) != 0)
        throw ErrnoError("cannot size the interface list");

    std::vector<char> buffer(size);
    // The interface list can change between the two sysctl calls; the
    // kernel then fails with ENOMEM and reports the larger size needed.
    for (int attempt = 0; ; ++attempt) {
        if (buffer.empty() || sysctl(mib, 6, &buffer[0], &size, NULL, 0) == 0)
            break;
        if (errno != ENOMEM || size <= buffer.size() || attempt >= 16)
            throw ErrnoError("cannot read the interface list");
        buffer.resize(size);
    }

    size_t offset = 0;
    while (offset + sizeof(unsigned short) <= size) {
        unsigned short messageLength;
        memcpy(&messageLength, &buffer[offset], sizeof(messageLength));
        // Walk by ifm_msglen: RTM_IFINFO2 messages are longer than
        // sizeof(struct if_msghdr2) because the kernel appends
        // addresses, and other message types are shorter. Stop on a
        // truncated tail rather than read past the buffer.
        if (messageLength < sizeof(unsigned short)
                || offset + messageLength > size)
            break;
        if (messageLength >= sizeof(struct if_msghdr2)) {
            struct if_msghdr2 message;
            memcpy(&message, &buffer[offset], sizeof(message));
            if (message.ifm_type == RTM_IFINFO2) {
                char name[IFNAMSIZ];
                if (if_indextoname(message.ifm_index, name) != NULL) {
                    const struct if_data64& data = message.ifm_data;
                    Statistics stats;
                    memset(&stats, 0, sizeof(stats));
                    stats.timestamp = timestamp;
                    stats.rx_bytes = data.ifi_ibytes;
                    stats.rx_packets = data.ifi_ipackets;
                    stats.rx_errs = data.ifi_ierrors;
                    stats.rx_drop = data.ifi_iqdrops;
                    stats.rx_multicast = data.ifi_imcasts;
                    stats.tx_bytes = data.ifi_obytes;
                    stats.tx_packets = data.ifi_opackets;
                    stats.tx_errs = data.ifi_oerrors;
                    stats.tx_drop = message.ifm_snd_drops;
                    stats.tx_multicast = data.ifi_omcasts;
                    // rx_fifo, rx_frame, rx_compressed and the tx
                    // equivalents have no BSD counterpart and stay zero.
                    Sample sample = { name, stats };
                    samples.push_back(sample);
                }
            }
        }
        offset += messageLength;
    }
    return samples;
}
#endif // __APPLE__

void Reader::update() {
#ifdef __APPLE__
    struct timeval now;
    struct timezone unused_timezone;
    gettimeofday(&now, &unused_timezone);
    applySamples(readInterfaceStatistics(now));
#else
    FILE* dev = fopen(PROC_NET_DEV, "r");
    if (!dev) throw ErrnoError("cannot open " PROC_NET_DEV);
    std::string content;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), dev)) > 0) content.append(buf, n);
    bool readError = (ferror(dev) != 0);
    fclose(dev);
    if (readError) throw ErrnoError("cannot read " PROC_NET_DEV);
    update(content);
#endif
}

void Reader::update(const std::string& devFileContents) {
    struct timeval now;
    struct timezone unused_timezone;
    gettimeofday(&now, &unused_timezone);     // single timestamp for the whole read

    applySamples(parseProcNetDev(devFileContents, now));
}

void Reader::applySamples(const SampleList& samples) {
    for (Interfaces::iterator i = interfaces_.begin(); i != interfaces_.end(); ++i)
        i->setUpdated(false);

    for (SampleList::const_iterator s = samples.begin(); s != samples.end(); ++s) {
        Interfaces::iterator interface
            = std::find_if(interfaces_.begin(), interfaces_.end(),
                           InterfaceNameMatchesPredicate(s->name));
        if (interface == interfaces_.end()) {
            interfaces_.push_back(Interface(s->name));
            interface = interfaces_.end();
            --interface;
        }
        interface->update(s->statistics);
    }
    interfaces_.remove_if(InterfaceNotUpdatedPredicate());
}

void Interface::setUpdated(bool updated) {
    updated_ = updated;
}

bool Interface::getUpdated() const {
    return updated_;
}

const std::string& Interface::getName() const {
    return name_;
}

double Interface::getReceiveSpeed() const {
    return receiveSpeed_;
}

double Interface::getTransmitSpeed() const {
    return transmitSpeed_;
}

double Interface::getReceiveMax() const {
    return receiveMax_;
}

double Interface::getTransmitMax() const {
    return transmitMax_;
}

const Reader::Interfaces& Reader::getInterfaces() const {
    return interfaces_;
}

} // namespace statistics
