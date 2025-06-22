#include "Version.h"

Version::Version()
    : version(0) {}

Version::Version(uint8_t major, uint8_t minor, uint8_t patch)
    : version((major << 16) | (minor << 8) | patch) {}

Version::Version(const String& other) {
    fromString(other);
}

Version::Version(const char* other) {
    fromString(other);
}

size_t Version::printTo(Print& printer) const {
    return printer.print(toString());
};

Version::Version(const Version& other)
    : version(other.version) {}

bool Version::operator==(const Version& other) const {
    return version == other.version;
}

bool Version::operator!=(const Version& other) const {
    return version != other.version;
}

bool Version::operator>=(const Version& other) const {
    return version >= other.version;
}
bool Version::operator<=(const Version& other) const {
    return version <= other.version;
}

Version& Version::operator=(const Version& other) {
    version = other.version;
    return *this;
}

Version& Version::operator=(const char* other) {
    fromString(other);
    return *this;
}

String Version::toString() const {
    return String((version >> 16) & 0x0F) + "." + String((version >> 8) & 0x0F) + "." + String(version & 0x0F);
}

void Version::fromString(const String& other) {
    int firstDot = other.indexOf('.');
    int secondDot = other.indexOf('.', firstDot + 1);

    if (firstDot == -1 || secondDot == -1) return;

    uint8_t major = other.substring(0, firstDot).toInt();
    uint8_t minor = other.substring(firstDot + 1, secondDot).toInt();
    uint8_t patch = other.substring(secondDot + 1).toInt();

    version = ((major << 16) | (minor << 8) | patch);
}
