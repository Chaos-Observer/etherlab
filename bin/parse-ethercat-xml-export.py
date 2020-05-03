#!/usr/bin/python
from xml.dom.minidom import parse, getDOMImplementation
import os
from pprint import PrettyPrinter

class Slave:
    def __init__(self, dom):
        return

class xml:
    def __init__(self, f):
        if isinstance(f, str):
            dom = parse(f)
            self.__node = dom.documentElement #childNodes[0]
        else:
            self.__node = f
          

    def getChildren(self,name):
        nodes = []
        for n in self.__node.childNodes:
            if n.nodeName == name:
                nodes.append(xml(n));
        return nodes

    def attribute(self,attr):
        return self.__node.getAttribute(attr)

    def name(self):
        return self.__node.nodeName

    def __nonzero__(self):
        return self.__node.firstChild != None

    def __int__(self):
        s = self.__node.firstChild.nodeValue
        if s.startswith('#x'):
            return int(s[2:], 16)
        else:
            return int(s)

    def __str__(self):
        if self.__node.firstChild:
            return self.__node.firstChild.nodeValue
        else:
            return ""

    def __getitem__(self,key):
        for n in self.__node.childNodes:
            if n.nodeName == key:
                return xml(n);

        emptydoc = getDOMImplementation().createDocument(None,"empty",None);

        return xml(emptydoc.documentElement);


class PdoEntry:
    def __init__(self,node):
        self.index = int(node["Index"])
        self.name = node["Name"]

        if not self.name:
            self.name = ""

        if self.index:
            self.subindex = int(node["SubIndex"])
        else:
            self.subindex = 0
        self.bitlen = int(node["BitLen"])

    def dump(self):
        print "            %i,%i,%i;   %% %s" % (self.index, self.subindex, self.bitlen, self.name)

class Pdo:
    def __init__(self, node):
        sm = node.attribute("Sm")
        if sm:
            self.sm = int(sm)
        else:
            self.sm = None
        self.index = int(node["Index"])
        self.name = node["Name"]
        self.entry = map(PdoEntry, node.getChildren("Entry"))

    def dump(self):
        print "        {%i, [    %% %s" % (self.index, self.name)
        for i in self.entry:
            i.dump()
        print "        ]},"

class SyncManager:
    def __init__(self,node,idx,txPdo,rxPdo):
        pdo = map(int, node.getChildren("Pdo"))
        txPdoIndex = map(lambda x: x.index, txPdo)
        rxPdoIndex = map(lambda x: x.index, rxPdo)
        self.tx_pdo = []
        self.rx_pdo = []
        self.index = idx
        for i in pdo:
            if i in txPdoIndex and not len(self.rx_pdo):
                self.tx_pdo.append(txPdo[txPdoIndex.index(i)])
            elif i in rxPdoIndex and not len(self.tx_pdo):
                self.rx_pdo.append(rxPdo[rxPdoIndex.index(i)])

    def dump(self):
        if len(self.tx_pdo):
            dir = 1
            pdo = self.tx_pdo
        elif len(self.rx_pdo):
            dir = 0
            pdo = self.rx_pdo
        else:
            return

        print "    {%i, %i, {" % (self.index, dir)
        for i in pdo:
            i.dump()
        print "    }},"

class CoE:
    def __init__(self, node):
        self.index = int(node["Index"])
        self.data = str(node["Data"])

    def dump(self):
        if self.index not in (7186,7187):
            array = str([int(self.data[i:(i+2)],16)
                for i in xrange(0, len(self.data), 2)])
            print "    %% %x %s" % (self.index, self.data)
            print "    %u, -1, 0, %s;" % (self.index, array)


class Slave:
    def __init__(self, node):
        info = node["Info"]
        self.name = info["Name"]
        self.vendor_id = int(info["VendorId"])
        self.product_code = int(info["ProductCode"])
        self.description = str(info["ProductRevision"])
        processData = node["ProcessData"]
        txPdo = map(Pdo, processData.getChildren("TxPdo"))
        rxPdo = map(Pdo, processData.getChildren("RxPdo"))
        self.sm = (SyncManager(processData["Sm0"], 0, txPdo, rxPdo),
                   SyncManager(processData["Sm1"], 1, txPdo, rxPdo),
                   SyncManager(processData["Sm2"], 2, txPdo, rxPdo),
                   SyncManager(processData["Sm3"], 3, txPdo, rxPdo))
        self.sdo = map(CoE, node["Mailbox"]["CoE"]["InitCmds"].getChildren("InitCmd"))

    def dump(self,idx):
        print "%% %s" % self.name
        print "rv.SlaveConfig(%i).vendor = %i;" % (idx, self.vendor_id)
        print "rv.SlaveConfig(%i).product = %i;" % (idx, self.product_code)
        print "rv.SlaveConfig(%i).description = '%s';" % (idx, self.description)
        print "rv.SlaveConfig(%i).sm = {" % idx
        for sm in self.sm:
            sm.dump()
        print "};"
        print
        print "rv.SlaveConfig(%i).sdo = {" % idx
        for sdo in self.sdo:
            sdo.dump()
        print "};"

if len(os.sys.argv) > 1:
    index = int(os.sys.argv[2])
    slave = Slave(xml(os.sys.argv[1])["Config"].getChildren("Slave")[index])
    slave.dump(1)
elif len(os.sys.argv):
    slaves = map(Slave, xml(os.sys.argv[1])["Config"].getChildren("Slave"));
    for s in slaves:
        s.dump(0)


# for node in config.getChildren("EtherCATConfig")
