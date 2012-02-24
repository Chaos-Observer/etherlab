function val = subsref(ei,index)
% SUBSREF Return properties from EtherCATInfo object
% 
% Two methods are available as subscription options:
% '.' (dot) notation:
%       Attempts to return a field as represented in the
%       EtherCATInfo XML file, e.g.
%       ei = EtherCATInfo('vendor.xml')
%       ei.Descriptions.Devices.Device
% () subscription notation
%       Attempts to locate a device with the arguments
%       e.g.
%       ei = EtherCATInfo('vendor.xml')
%       ei({'EL1004' 'EL1008'},0) tries to locate the devices with
%           Type == {'EL1004' 'EL1008'} and RevisionNo == 0
%       ei(hex2dec('03EC3052')) returns all devices where
%           ProduceCode == #x03EC3052
% {} subscription notation
%       Returns the slaves at the requested index. Allowed is a
%       single number or ':',e.g
%       ei{':'} or ei{3}
val = ei;
for i = 1:length(index)
    %idx = index(i)
    switch index(i).type
        case '{}'
            if index(i).subs{1} == ':'
                val = ei;
            else
                val = ei;
                val.Descriptions.Devices.Device = ...
                    ei.Descriptions.Devices.Device(index(i).subs{1});
            end
        case '()'
            if i == 1 && length(index.subs)
                if index.subs{1} == ':'
                    val = ei;
                else
                    rev = [];
                    pc = [];
                    if length(index.subs) >= 2
                        rev = index.subs{2};
                    end
                    if length(index.subs) >= 1
                        pc = index.subs{1};
                    end
                    val = getDevice(ei,pc,rev);
                end
            elseif index(i).subs{1} == ':'
            elseif index(i).subs{1} <= length(val)
                val = val(index(i).subs{1});
            else
                error('Index exceeds matrix dimensions.');
            end
        case '.'
            val = val.(index(i).subs);
        otherwise
            error('Method %s is not implemented by %s', index(i).type, class(val));
    end
end
