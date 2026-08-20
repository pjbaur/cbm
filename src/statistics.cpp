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

#define PROC_NET_DEV "/proc/net/dev"

namespace statistics {

Interface::Interface(const std::string& name)
    : name_(name), updated_(false), initialized_(false),
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
    static unsigned int count = 0;

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
        count++;
        return;
    }

    receiveSpeed_ = (x1.rx_bytes - x0.rx_bytes) / timeDelta;
    transmitSpeed_ = (x1.tx_bytes - x0.tx_bytes) / timeDelta;

    count++;

    // Waits some iterations before calculating max speeds.
    // This avoids presenting wrong initial peaks.
    if (count < 8)
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
                   "%Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu "
                   "%Lu %Lu %Lu %Lu %Lu %Lu %Lu %Lu",
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

void Reader::update() {
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
}

void Reader::update(const std::string& devFileContents) {
    struct timeval now;
    struct timezone unused_timezone;
    gettimeofday(&now, &unused_timezone);     // single timestamp for the whole read

    const SampleList samples = parseProcNetDev(devFileContents, now);

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
