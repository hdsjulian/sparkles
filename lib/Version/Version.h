#pragma once

#include <Arduino.h>
#include <Printable.h>

class Version : public Printable {
  public:
    Version();
    Version(uint8_t major, uint8_t minor, uint8_t patch);
    Version(const Version& other);
    Version(const String& other);
    Version(const char* other);

    bool operator==(const Version& other) const;
    bool operator!=(const Version& other) const;
    bool operator>=(const Version& other) const;
    bool operator<=(const Version& other) const;

    Version& operator=(const Version& other);
    Version& operator=(const char* other);

    String toString() const;
    void   fromString(const String& other);

  protected:
    size_t printTo(Print& printer) const;

  protected:
    uint32_t version;
};
