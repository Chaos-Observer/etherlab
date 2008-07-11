function ei = appendDevice(ei, val)
% APPENDDEVICE Append devices
%
% Synopsis: ei = appendDevices(ei, val)
% Append devices from val to ei and return the value
% val can either be a list of Devices or an EtherCATInfo 
% object

if class(ei) ~= class(val)
    error('Only objects of type %s can be appended',...
        class(ei));
end

if ei.Vendor.Id ~= val.Vendor.Id
    error('VendorId mismatch')
end
ei.Descriptions.Devices.Device = ...
    [ei.Descriptions.Devices.Device, val.Descriptions.Devices.Device];
