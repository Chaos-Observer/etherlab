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
    invalidDocument();
end


%% Try to get the VendorId - this is required
try
    VendorElem = root.getElementsByTagName('Vendor').item(0);
    VendorIdTxt = VendorElem.getElementsByTagName('Id').item(0);
    ei.Vendor.Id = fromHexString(VendorIdTxt.getTextContent.trim);
catch
end

if isempty(ei.Vendor.Id)
    invalidDocument();
end

%% Get the devices
try
    % These really should be separate classes of :
    %   * Descriptions
    %   * Devices
    descriptionsElem = root.getElementsByTagName('Descriptions').item(0);
    ei.Descriptions = [];
    DevicesElem = descriptionsElem.getElementsByTagName('Devices').item(0);
    ei.Descriptions.Devices = [];
    DeviceElem = DevicesElem.getElementsByTagName('Device');
catch
    invalidDocument();
end

idx = 1;
for i = 1:DeviceElem.getLength
    dev = XML_ParseSlave(DeviceElem.item(i-1));
    if ~isempty(dev.Type.ProductCode) && ~isempty(dev.Type.RevisionNo)
        ei.Descriptions.Devices.Device(idx) = dev;
        idx = idx + 1;
    end
end

%%
function invalidDocument
error('XML file is not a valid EtherCATInfo Document');
