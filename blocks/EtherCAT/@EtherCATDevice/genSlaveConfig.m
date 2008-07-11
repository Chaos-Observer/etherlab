function sc = genSlaveConfig(dev)
% SlaveConfig: Return the slave as a structure
%
% Synopsis: sc = SlaveConfig(device,VendorId)

if nargin < 1
    help genSlaveConfig
    error('Not enough input arguments');
end
'hello'

fn = fieldnames(dev);
for i = 1:length(fn)
    sc.(fn{i}) = dev.(fn{i});
end

