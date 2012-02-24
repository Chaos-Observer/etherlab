function dev = XML_ParseSlave(xml)

dev.Type = struct;
dev.HideType = [];
dev.Sm = struct;
dev.TxPdo = struct;
dev.RxPdo = struct;
dev.Dc = [];

if ~nargin
    return
end

%% Get the ProductCode, RevisionNo and Type 
typeElem = xml.getElementsByTagName('Type').item(0);
if isempty(typeElem)
    InvalidDoc(xml,'XML_ParseSlave','ElementNotFound','Type');
end
dev.Type.ProductCode = readHexDecValue(typeElem,1,'ProductCode');
dev.Type.RevisionNo  = readHexDecValue(typeElem,1,'RevisionNo');
dev.Type.TextContent = char(typeElem.getTextContent.trim);

if isempty(dev.Type.ProductCode) || isempty(dev.Type.RevisionNo)
    dev = [];
    return
end

%% Parse HideType (optional)

hide = xml.getElementsByTagName('HideType');
dev.HideType = repmat(0,1,hide.getLength);
for i = 1:hide.getLength
    dev.HideType(i) = readHexDecValue(hide.item(i-1),1,'RevisionNo');
end

%% Parse the SyncManager elements

element = xml.getElementsByTagName('Sm');
idx = 1;
dev.Sm = repmat(struct('Virtual',0,'ControlByte',[]),1,element.getLength);
for i = 1:element.getLength
    sm = element.item(i-1);

    % Since Dc/OpMode can also have <Sm> elements, have to be careful
    % here
    if (sm.getParentNode ~= xml)
        continue
    end
    
    dev.Sm(idx).Virtual = readBooleanValue(sm,'Virtual');
    if ~dev.Sm(idx).Virtual
        dev.Sm(idx).ControlByte = readHexDecValue(sm,1,'ControlByte');
    end
    idx = idx + 1;
end
dev.Sm = dev.Sm(1:idx-1);

%% Parse TxPdo and RxPdo

tx_pdos = xml.getElementsByTagName('TxPdo');
dev.TxPdo = repmat(XML_ParsePdo,1,tx_pdos.getLength);
for i = 1:tx_pdos.getLength
    TxPdo = tx_pdos.item(i-1);
    dev.TxPdo(i) = XML_ParsePdo(TxPdo);
end

rx_pdos = xml.getElementsByTagName('RxPdo');
dev.RxPdo = repmat(XML_ParsePdo,1,rx_pdos.getLength);
for i = 1:rx_pdos.getLength
    RxPdo = rx_pdos.item(i-1);
    dev.RxPdo(i) = XML_ParsePdo(RxPdo);
end

dev.Dc = XML_ParseDc(xml);
