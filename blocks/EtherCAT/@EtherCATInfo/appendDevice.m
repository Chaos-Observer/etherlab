function ei = appendDevice(ei, val)
% APPENDDEVICE Append devices
%
% Synopsis: ei = appendDevices(ei, val)
% Append devices from val to ei and return the value
% val can either be a list of Devices or an EtherCATInfo 
% object

if class(ei) == class(val)
    if ei.VendorId ~= val.VendorId
        error('VendorId mismatch')
    end
    val = val.Descriptions.Devices.Device;
end
offset = length(ei.Descriptions.Devices.Device);
for i = 1:length(val)
    ei.Descriptions.Devices.Device(offset+i) = val(i);
end