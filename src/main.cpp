#include <map>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include "udevmonitor.hpp"
#include "../3rdparty/iniparser/src/iniparser.h"
#include "ut.hpp"
#include <libgen.h>

std::string configDir;
virConnectPtr virt;
std::string lastOpPre;
void libvirtErrorHandler(void * userData, virErrorPtr err)
{
    std::cerr << lastOpPre << "libvirt: "
        << ( (err && err->message ) ? err->message : "Unknown error" )
        << std::endl;
}

void virtPrintError(const char *w)
{
    virErrorPtr err = virGetLastError();

    std::cerr << w << " : "
        << ( (err && err->message ) ? err->message : "Unknown error" )
        << std::endl;

}

struct HubConfig {
    std::string name;
    std::string vendor;
    std::string product;
    std::string domain;
};

class HubContainer
{
public:
    HubContainer(const HubConfig &config, const UdevMonitor::Device *dev)
        : m_dev(dev)
        , config(config)
    {
        lastOpPre = pre();
        name = config.name + "-" + m_dev->sysname;


        std::cerr << pre()  << "new hub" << std::endl;


        virDomainPtr dom = virDomainLookupByName(virt, name.c_str());
        if (dom) {
            virDomainDestroy(dom);
            virDomainUndefine(dom);
            virDomainFree(dom);
        }

        std::string xmlDesc = config.domain;
        strReplace(xmlDesc, std::string("{{NAME}}"), name);
        strReplace(xmlDesc, std::string("{{CONFIGDIR}}"), configDir);
        dom = virDomainDefineXML(virt, xmlDesc.c_str());
        if (!dom) {
            std::cerr <<  pre() << "unable to define domain for " <<  m_dev->sysname << std::endl;
        }
        int r = virDomainCreate(dom);
        if (r = 0) {
            std::cerr <<  pre() << "unable to start domain for " <<  m_dev->sysname << std::endl;
        }
        virDomainFree(dom);

        lastOpPre = "[??]";
    }
    ~HubContainer()
    {
        lastOpPre = pre();
        std::cerr << pre()  << "destroying hub" << std::endl;
        virDomainPtr dom = virDomainLookupByName(virt, name.c_str());
        virDomainDestroy(dom);
        virDomainUndefine(dom);
        virDomainFree(dom);
    }

    std::string pre() const {
        return "[" + config.name + ":" +  m_dev->sysname + "] ";
    }
    void reAttach() {
        lastOpPre = pre();
        std::cerr << pre() <<  "re-attaching "  << std::endl;
        virDomainPtr dom = virDomainLookupByName(virt, name.c_str());
        for (std::map<std::string, std::string>::const_iterator i
                = attachedNodes.begin();  i != attachedNodes.end(); i++) {
            virDomainAttachDevice(dom, i->second.c_str());
        }
    }

    virtual void onDeviceAdded   (const UdevMonitor::Device *dev) {
        lastOpPre = pre();
        virDomainPtr dom = virDomainLookupByName(virt, name.c_str());
        if (!dev->node.empty()) {
            std::cerr <<  pre() <<  " ++ "  << dev->node <<  std::endl;


            std::string xml =
                "    <hostdev mode='capabilities' type='misc' >"
                "      <source>"
                "        <char>" + dev->node + "</char>"
                "      </source>"
                "    </hostdev>"
                ;
            attachedNodes[m_dev->sysname + dev->node] = xml;
            int r = virDomainAttachDevice(dom, xml.c_str());
        }
        virDomainFree(dom);
    }
    virtual void onDeviceRemoved (const UdevMonitor::Device *dev) {
        lastOpPre = pre();
        virDomainPtr dom = virDomainLookupByName(virt, name.c_str());
        if (!dev->node.empty()) {
            std::cerr <<  pre() << " -- " << dev->node <<  std::endl;
            std::string xml =
                "    <hostdev mode='capabilities' type='misc' >"
                "      <source>"
                "        <char>" + dev->node + "</char>"
                "      </source>"
                "    </hostdev>"
                ;
            attachedNodes.erase(attachedNodes.find(m_dev->sysname + dev->node));
            int r = virDomainDetachDevice(dom, xml.c_str());
        }
        virDomainFree(dom);
    }

    std::string name;
    std::map<std::string, std::string> attachedNodes;
    const UdevMonitor::Device *m_dev;
    const HubConfig &config;
};



class HubController : public UdevMonitor
{
public:
    std::vector<HubConfig> config;

    std::map<const UdevMonitor::Device *, HubContainer*> managedHubs;

    HubController()
        : UdevMonitor ()
    {
    }

    virtual void onDeviceAdded   (const UdevMonitor::Device *dev) {

        const UdevMonitor::Device *parent = parentDevice(dev);
        while (parent) {
            HubContainer *container = managedHubs[parent];
            if (container) {
                container->onDeviceAdded(dev);
                return; //dont try to manage it here, even if it _is_ a hub
            }
            parent = parentDevice(parent);
        }


        for (int i = 0 ; i < config.size(); i++) {
            const HubConfig &conf = config[i];

            if (conf.vendor == dev->vendorId && conf.product == dev->productId) {
                if (!managedHubs.count(dev)) {
                    managedHubs[dev] = new HubContainer(conf, dev);
                }
            }
        }


    }
    virtual void onDeviceRemoved (const UdevMonitor::Device *dev) {
        if (managedHubs[dev]) {
            delete managedHubs[dev];
            managedHubs.erase(managedHubs.find(dev));
        }

        const UdevMonitor::Device *parent = parentDevice(dev);
        if (parent) {
            HubContainer *container = managedHubs[parent];
            if (container) {
                container->onDeviceRemoved(dev);
                return;
            }
        }
    }

    void virpolli(int fd)
    {
        int event = 0, detail = 0;
        std::string name;
        std::string::size_type nameSize = 0;

        read(fd, &event,    sizeof(int));
        read(fd, &detail,   sizeof(int));
        read(fd, &nameSize, sizeof(std::string::size_type));
        name.resize(nameSize);
        read(fd, (void*)name.data(), nameSize);


        for (std::map<const UdevMonitor::Device *, HubContainer*>::const_iterator i
                = managedHubs.begin();  i != managedHubs.end(); i++) {
            if (!i->second)
                continue;
            if (i->second->name == name) {
                if (event == VIR_DOMAIN_EVENT_STARTED) {
                    i->second->reAttach();
                }
            }
        }
    }
private:
    std::string m_vendor;
    std::string m_product;

};


void usage(int e)
{
    std::cerr << "arguments: path/to/settings.ini\n";
    exit(e);
}

int vircoms[2];

static int myDomainEventCallback1(virConnectPtr conn ,
        virDomainPtr dom,
        int event,
        int detail,
        void *opaque )
{
    std::string name = virDomainGetName(dom);
    std::string::size_type nameSize = name.size();
    write(vircoms[1], &event,    sizeof(int));
    write(vircoms[1], &detail,   sizeof(int));
    write(vircoms[1], &nameSize, sizeof(std::string::size_type));
    write(vircoms[1], name.c_str(), nameSize);
    return 0;
}


void* virpoll(void*)
{

    while (true) {
        if (virEventRunDefaultImpl())
            virtPrintError("virEventRunDefaultImpl");
    }
}

int main(int argc, char **argv)
{

    std::cerr << "[main] hi" << std::endl;

    if (argc < 2)
        usage(2);

    char *configFile = realpath(argv[1], 0);
    if (!configFile)
        usage(3);

    configDir = dirname(strdup(configFile));

    dictionary *dict = iniparser_load(configFile);
    if (!dict)
        usage(3);

    std::string virturl = iniparser_getstring(dict, "libvirt:url", (char*)("lxc://"));

    HubController  hdm;
    int sections = iniparser_getnsec(dict);
    for (int i = 0; i < sections; i++) {
        std::string secname = iniparser_getsecname(dict, i);

        if (secname == "libvirt")
            continue;

        std::cerr << "[main] config for " << secname << std::endl;

        HubConfig cfg;
        cfg.name = secname;

        const char *t = iniparser_getstring(dict, (secname + ":vendor").c_str(), 0);
        if (!t)
            throw std::runtime_error(("missing " + secname + "/vendor").c_str());
        cfg.vendor = t;

        t = iniparser_getstring(dict, (secname + ":product").c_str(), 0);
        if (!t)
            throw std::runtime_error(("missing " + secname + "/product").c_str());
        cfg.product = t;

        t = iniparser_getstring(dict, (secname + ":domain").c_str(), 0);
        if (!t)
            throw std::runtime_error(("missing " + secname + "/domain").c_str());

        const char *t2 = realpath(t, 0);
        if (t[0] != '/') {
            std::string rp = configDir + "/"  + t;
            t2 = realpath(rp.c_str(), 0);
        }

        if (!t2) {
            throw std::runtime_error((std::string("cannot find domain file ") + t).c_str());
        }
        std::ifstream ifs(t2);
        if (!ifs.is_open()) {
            throw std::runtime_error((std::string("cannot open domain file ")+ t2).c_str());
        }

        std::string str((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        cfg.domain = str;

        hdm.config.push_back(cfg);
    }


    std::cerr << "[main] connecting virt" << std::endl;

    if (virEventRegisterDefaultImpl() < 0)
        virtPrintError("virEventRegisterDefaultImpl");

    virt = virConnectOpen("lxc://");
    if (virt == NULL) {
        virtPrintError("virConnectOpen");
        exit (8);
    }

    virSetErrorFunc(0, libvirtErrorHandler);



    if (virConnectSetKeepAlive(virt, 5, 3) < 0)
        virtPrintError("virConnectSetKeepAlive");

    if (virConnectDomainEventRegisterAny(virt,
            NULL,
            VIR_DOMAIN_EVENT_ID_LIFECYCLE,
            VIR_DOMAIN_EVENT_CALLBACK(myDomainEventCallback1),
            NULL, NULL
            ) < 0)  virtPrintError("virConnectDomainEventRegisterAny");


    if (pipe(vircoms))
        throw std::runtime_error("no pipe");
    if (pipe(vircoms))
        throw std::runtime_error("no pipe");

    pthread_t pt;
    pthread_create(&pt, NULL, virpoll, NULL);

    hdm.init();

    std::cerr << "[main] ready" << std::endl;
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(hdm.fd(), &fds);
        FD_SET(vircoms[0], &fds);
        int maxfd = 0;
        maxfd = std::max(maxfd, hdm.fd());
        maxfd = std::max(maxfd, vircoms[0]);

        int ret = select(maxfd + 1, &fds, NULL, NULL, NULL);
        if (ret > 0) {
            if (FD_ISSET(hdm.fd(), &fds)) {
                hdm.activate();
            }
            if (FD_ISSET(vircoms[0], &fds)) {
                hdm.virpolli(vircoms[0]);
            }
        }
    }

}
