Libvirt Hub Agent
=================

spawns a virt domain per usb hub and attaches all devices on that hub to the domain

Config
=================

hub-agent takes one argument: an ini file, which looks like this:

    [libvirt]
    url = lxc://
    
    [mything]
    vendor  = 1d6b
    product = 0002
    domain  = domain.example.xml


the libvirt section is for libvirt related settings,
such as the url to connect to, defaulting to lxc://

Every other section is going to be part of the domain name
in the format section-sysname. for example: mything-usb1-4.3

The section needs to have vendor= and product= of the usb hub.
for every hub detected with those properties, a new domain will be spawn.

As domain definition, the agent will take the file specified in domain=
relative to the ini file (or an absolute path)  and replace these strings:

    {{NAME}}       - the name of the domain, as specified above
    {{CONFIGDIR}}  - path that the ini file was in



Building
==================


docker pull  alanfranz/drb-epel-6-x86-64
