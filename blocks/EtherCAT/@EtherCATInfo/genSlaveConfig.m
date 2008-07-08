function sc = genSlaveConfig(dev, ProductCode, RevisionNo)
% SlaveConfig: Return the slave as a structure
%
% Synopsis: sc = SlaveConfig(device)
slave = getDevice(dev,ProductCode, RevisionNo);

sc = genSlaveConfig(slave,dev.VendorId);
