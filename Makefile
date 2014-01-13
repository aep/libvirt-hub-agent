TARGET  := bin/libvirt-hub-agent
SOURCES := \
	src/udevmonitor.cpp \
	src/main.cpp  \
	3rdparty/iniparser/src/iniparser.c \
	3rdparty/iniparser/src/dictionary.c \


$(TARGET): $(SOURCES)
	@mkdir -p $(dir $(TARGET))
	$(CXX) -g $^ -o $@ -ludev -lvirt -lpthread


clean:
	rm -f $(TARGET)
	rm -rf rpmbuild
	rm -f ${RPM}


#BASE := layilel/rpmbuild-f20
BASE := fedora


rpm:
	docker run -w ${PWD} -v ${PWD}:${PWD} ${BASE} \
	make rpmi

rpmi:
	yum install -y yum-utils rpm-build make gcc
	yum-builddep -y $${PWD}/hub-agent.spec
	rpmbuild  \
		--define "_topdir $${PWD}/rpmbuild" \
		--define "_builddir ./" \
		-bb $${PWD}/hub-agent.spec
	cp rpmbuild/RPMS/*/*.rpm .

DESTDIR ?= "/"
install:
	install -d $(DESTDIR)/usr/
	install -d $(DESTDIR)/usr/bin/
	install -d $(DESTDIR)/etc/
	install -d $(DESTDIR)/etc/libvirt-hub-agent/
	install -d $(DESTDIR)/usr/lib/
	install -d $(DESTDIR)/usr/lib/systemd/
	install -d $(DESTDIR)/usr/lib/systemd/system/
	install -m 755 $(TARGET) $(DESTDIR)/usr/bin/
	install -m 644 etc/domain.example.xml $(DESTDIR)/etc/libvirt-hub-agent/
	install -m 644 etc/agent.ini $(DESTDIR)/etc/libvirt-hub-agent/
	install -m 755 etc/init.example.sh  $(DESTDIR)/etc/libvirt-hub-agent/
	install -m 755 etc/libvirt-hub-agent.service  $(DESTDIR)/usr/lib/systemd/system/
