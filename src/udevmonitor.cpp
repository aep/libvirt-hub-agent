#include <exception>
#include <stdexcept>
#include "udevmonitor.hpp"

UdevMonitor::UdevMonitor()
{
    udev =  udev_new();
    if (!udev)
        throw std::runtime_error("udev_new");
    mon = udev_monitor_new_from_netlink(udev, "udev");
    if (!mon)
        throw std::runtime_error("udev_monitor_new_from_netlink");

//    if (udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device") != 0)
//        throw std::runtime_error("udev_monitor_filter_add_match_subsystem_devtype");

    if (udev_monitor_enable_receiving(mon) != 0)
        throw std::runtime_error("udev_monitor_enable_receiving");

}

UdevMonitor::~UdevMonitor()
{
    udev_unref(udev);
}

int UdevMonitor::fd() const
{
    return udev_monitor_get_fd(mon);
}

void UdevMonitor::m_addDev(struct udev_device *dev)
{
    if (!dev) {
        std::cerr << "{udev} dev missing oO" << std::endl;
        return;
    }

    const char *sysname = udev_device_get_sysname(dev);
    const char *devpath = udev_device_get_devpath(dev);
    if (!devpath) {
        std::cerr << "{udev} missing devpath\n";
        return;
    }

    std::string id = devpath;

    if (devices.count(id)) {
        std::cerr << "{udev} device already added\n";
        return;
    }

    const char *node   = udev_device_get_devnode(dev);
    mode_t dmode = S_IRWXU | S_IRWXG | S_IRWXO;
    if (strcmp(udev_device_get_subsystem(dev), "block") == 0)
        dmode |= S_IFBLK;
    else
        dmode |= S_IFCHR;
    dev_t devt = udev_device_get_devnum(dev);

    const char *vid = udev_device_get_sysattr_value(dev, "idVendor");
    const char *pid = udev_device_get_sysattr_value(dev, "idProduct");

    Device *dc = new Device;

    dc->udev    = dev;
    if (sysname)
        dc->sysname = sysname;
    if (node)
        dc->node    = node;
    dc->mode = dmode;
    dc->devt = devt;
    if (vid)
        dc->vendorId  = vid;
    if (pid)
        dc->productId = pid;

    devices[id] = dc;


    onDeviceAdded(dc);


}

void UdevMonitor::m_rmDev(struct udev_device *dev)
{
    if (!dev) {
        std::cerr << "{udev} m_rmDev dev missing oO" << std::endl;
        return;
    }

    const char *devpath_ = udev_device_get_devpath(dev);
    if (!devpath_) {
        std::cerr << "{udev} m_rmDev missing devpath\n";
        return;
    }

    std::string id = devpath_;

    if (!devices.count(id)) {
//        std::cerr << "{udev} m_rmDev device never added  " << id << std::endl;;
        return;
    }




    devices[id];

    Device *dc = devices[id];

    onDeviceRemoved(dc);

    devices.erase(devices.find(id));

    udev_device_unref(dc->udev);
    delete dc;
}

void UdevMonitor::init()
{
    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "usb");
    udev_enumerate_add_match_subsystem(enumerate, "usb-serial");
    udev_enumerate_add_match_subsystem(enumerate, "tty");
    udev_enumerate_scan_devices(enumerate);


    struct udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);

    struct udev_list_entry *dev_list_entry;
    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path = udev_list_entry_get_name(dev_list_entry);
        struct udev_device * dev = udev_device_new_from_syspath(udev, path);
        m_addDev(dev);
    }
    udev_enumerate_unref(enumerate);

}

int UdevMonitor::activate()
{
    /* Make the call to receive the device.
       select() ensured that this will not block. */
    struct udev_device *dev = udev_monitor_receive_device(mon);
    if (dev) {
        const char *action  = udev_device_get_action(dev);

        if (strcmp(action, "add") == 0) {
            m_addDev(dev);
        } else if (strcmp(action, "remove") == 0) {
            m_rmDev(dev);
        } else {
            std::cerr << "{udev} action: " << action << std::endl;
        }
    }
}

const UdevMonitor::Device * UdevMonitor::parentDevice(const UdevMonitor::Device *d)
{
    if (!d)
        return 0;
    if (!d->udev)
        return 0;
    struct udev_device *p= udev_device_get_parent(d->udev);
    if (!p)
        return 0;
    const char *psn = udev_device_get_devpath(p);
    if (!psn) {
//        udev_device_unref(p);
        return 0;
    }

    UdevMonitor::Device *r =  devices[psn];

//    udev_device_unref(p);

    return r;
}
