function dev = ParseXML(xml)

dev.Type = ...
    struct('ProductCode', [], 'RevisionNo', [], 'TextContent', '');
dev.HideType = [];
dev.Sm = repmat(struct('Virtual',0,'ControlByte',[]),1,0);
dev.TxPdo = repmat(XML_ParsePdo,1,0);
dev.RxPdo = repmat(XML_ParsePdo,1,0);

if ~nargin
    return
end

%% Get the ProductCode, RevisionNo and Type 
try
    typeElem = xml.getElementsByTagName('Type').item(0);
    dev.Type.ProductCode = ...
        fromHexString(typeElem.getAttribute('ProductCode'));
    dev.Type.RevisionNo = ...
        fromHexString(typeElem.getAttribute('RevisionNo'));
    dev.Type.TextContent = char(typeElem.getTextContent.trim);
catch
end

if isempty(dev.Type.ProductCode) || isempty(dev.Type.RevisionNo)
    return
end

%% Parse HideType

hide = xml.getElementsByTagName('HideType');
for i = 1:hide.getLength
    if ~hide.item(i-1).hasAttribute('RevisionNo')
        continue
    end
    dev.HideType(i) = fromHexString(hide.item(i-1).getAttribute('RevisionNo'));
end

%% Parse the SyncManager elements

element = xml.getElementsByTagName('Sm');
for i = 1:element.getLength
    sm = element.item(i-1);
    
    if sm.hasAttribute('ControlByte')
        dev.Sm(i).ControlByte = ...
            fromHexString(sm.getAttribute('ControlByte'));
    end
    
    if sm.hasAttribute('Virtual')
        val = sm.getAttribute('Virtual');
        if val.equalsIgnoreCase('true') || val.equals('1')
            dev.Sm(i).Virtual = 1;
        end
    end
end

%% Parse TxPdo and RxPdo

tx_pdos = xml.getElementsByTagName('TxPdo');
for i = 1:tx_pdos.getLength
    TxPdo = tx_pdos.item(i-1);
    dev.TxPdo(i) = XML_ParsePdo(TxPdo);
end

rx_pdos = xml.getElementsByTagName('RxPdo');
for i = 1:rx_pdos.getLength
    RxPdo = rx_pdos.item(i-1);
    dev.RxPdo(i) = XML_ParsePdo(RxPdo);
end

%%
function invalidDocument
error('XML file is not a valid EtherCATInfo Document');
