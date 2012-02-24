function ei = ParseXML(xmlfile)

ei.Vendor.Id = [];
ei.Descriptions.Devices.Device = repmat(XML_ParseSlave,1,0);

if ~nargin
    return
end

%% Open the file and get its root element
tree = xmlread(xmlfile);
root = tree.getDocumentElement;

if ~strcmp(root.getNodeName, 'EtherCATInfo')
    InvalidDoc(root,'ParseXML','RootNotFound');
end


%% Try to get the VendorId - this is required
VendorElem = root.getElementsByTagName('Vendor').item(0);
if isempty(VendorElem)
    InvalidDoc(root,'ParseXML','ElementNotFound','Vendor')
end

VendorIdTxt = VendorElem.getElementsByTagName('Id').item(0);
if isempty(VendorIdTxt)
    InvalidDoc(VendorElem,'ParseXML','ElementNotFound','Id')
end

ei.Vendor.Id = readHexDecValue(VendorIdTxt,0);

% Descend into <EtherCATInfo><Descriptions><Devices>
descriptionsElem = root.getElementsByTagName('Descriptions').item(0);
if isempty(descriptionsElem)
    InvalidDoc(root,'ParseXML','ElementNotFound','Descriptions');
end

DevicesElem = descriptionsElem.getElementsByTagName('Devices').item(0);
if isempty(DevicesElem)
    InvalidDoc(descriptionsElem,'ParseXML','ElementNotFound','Devices');
end

% Go throuch every <Device>
ei.Descriptions = [];
ei.Descriptions.Devices = [];
DeviceElem = DevicesElem.getElementsByTagName('Device');
idx = 1;
for i = 1:DeviceElem.getLength
    dev = XML_ParseSlave(DeviceElem.item(i-1));
    if ~isempty(dev)
        ei.Descriptions.Devices.Device(idx) = dev;
        idx = idx + 1;
    end
end
