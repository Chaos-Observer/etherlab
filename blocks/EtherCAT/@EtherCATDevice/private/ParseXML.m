function dev = ParseXML(xml)

dev.ProductCode = 0;
dev.Type = '';
dev.RevisionNo = 0;
dev.HideType = [];
dev.Sm = struct('Input',[],'Output',[],'Virtual',[]);
dev.TxPdo = repmat(XML_ParsePdo,1,0);
dev.RxPdo = repmat(XML_ParsePdo,1,0);

if ~nargin
    return
end

%% Get the ProductCode, RevisionNo and Type 
try
    typeElem = xml.getElementsByTagName('Type').item(0);
    dev.ProductCode = fromHexString(typeElem.getAttribute('ProductCode'));
    dev.Type = char(typeElem.getTextContent.trim);
    dev.RevisionNo = fromHexString(typeElem.getAttribute('RevisionNo'));
catch
end

if isempty(dev.ProductCode) || isempty(dev.RevisionNo)
    return
end

%% Parse HideType

hide = xml.getElementsByTagName('HideType');
for i = 1:hide.getLength
    dev.HideType(i) = fromHexString(hide.item(i-1).getAttribute('RevisionNo'));
end

%% Parse the SyncManager elements

element = xml.getElementsByTagName('Sm');
for SmIdx = 0:element.getLength-1
    sm = element.item(SmIdx);
    if sm.hasAttribute('ControlByte')
        str = sm.getAttribute('ControlByte');
        ControlByte = uint8(fromHexString(str));

        % (Bit1,Bit0) of ControlByte must equal (0,0)
        if bitand(ControlByte,3)
            continue
        end
        
        % (Bit3,Bit2) is the direction:
        %   (0,0) Input for master
        %   (0,1) Output for master
        switch bitand(ControlByte,12)
            case 0
                dev.Sm.Input = [dev.Sm.Input SmIdx];
            case 4
                dev.Sm.Output = [dev.Sm.Output SmIdx];
            otherwise
                error('Unknown SyncManager ControlByte %u', ControlByte);
        end
    elseif sm.hasAttribute('Virtual')
        val = sm.getAttribute('Virtual');
        if val.equalsIgnoreCase('true')
            dev.Sm.Virtual = [dev.Sm.Virtual SmIdx];
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
