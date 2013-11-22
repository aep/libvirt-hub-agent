#ifndef UDEVMONITORYSAPKMASDSAD_HMILCH
#define UDEVMONITORYSAPKMASDSAD_HMILCH

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <algorithm>

class UdevMonitor
{
public:
    struct Device
    {
        struct udev_device *udev;
        std::string sysname;
        std::string node;
        mode_t mode;
        dev_t devt;
        std::string vendorId;
        std::string productId;
    };

    UdevMonitor();
    ~UdevMonitor();

    void init();

    virtual void onDeviceAdded   (const Device *dev) = 0;
    virtual void onDeviceRemoved (const Device *dev) = 0;

    const Device *parentDevice(const Device *);

    int activate();
    int fd() const;

private:

    void m_addDev(struct udev_device *);
    void m_rmDev(struct udev_device *);

    struct udev *udev;
    struct udev_monitor *mon;
    std::map<std::string, Device *> devices;
};

#endif
