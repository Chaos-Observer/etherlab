function sc = genSlaveConfig(ei, ProductCode, RevisionNo)
% SlaveConfig: Return the slave as a structure
%
% Synopsis: sc = SlaveConfig(device)

ei = getDevice(ei,ProductCode, RevisionNo);

sc.Vendor = ei.Vendor;
sc.Descriptions = ei.Descriptions;
