function sc = genSlaveConfig(dev,VendorId)
% SlaveConfig: Return the slave as a structure
%
% Synopsis: sc = SlaveConfig(device,VendorId)

if nargin < 2
    help genSlaveConfig
    error('Not enough input arguments');
end

fn = fieldnames(dev);
for i = 1:length(fn)
    sc.(fn{i}) = dev.(fn{i});
end

sc.VendorId = VendorId;
