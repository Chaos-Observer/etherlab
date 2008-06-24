function [VendorId name RevisionNo TxPdo RxPdo] = ...
    getSlaveDescription(xmlfile, ProductCode, RevisionNo, PdoList)

    tree = xmlread(xmlfile);

    info = tree.getDocumentElement;

    if ~strcmp(info.getNodeName, 'EtherCATInfo')
        error('getSlaveInfo:getSlaveInfo:elementNotFound',...
                'XML Document does not have root element <EtherCATInfo>');
    end

    parent = '/EtherCATInfo';

    try
        vendor = info.getElementsByTagName('Vendor').item(0);
        VendorId = vendor.getElementsByTagName('Id').item(0);
        VendorId = fromHexString(VendorId.getTextContent.trim);
        descriptions = info.getElementsByTagName('Descriptions');
        devices = descriptions.item(0).getElementsByTagName('Devices');
        device = devices.item(0).getElementsByTagName('Device');
        if ~device.getLength
            error(' '); %Generate empty error, is caught below
        end
    catch
        p = '';
        c = '';
        try
            if ~descriptions.getLength
                p = parent;
                c = 'Descriptions';
            elseif ~devices.getLength
                p = [parent '/Descriptions'];
                c = 'Devices';
            elseif ~device.getLength
                p = [parent '/Descriptions/Devices'];
                c = 'Device';
            end
        catch
        end

        error('getSlaveInfo:getSlaveInfo:elementNotFound',...
            'XML element <%s> does not have child <%s>.', p,c);
    end

    parent = [parent '/Descriptions/Devices/Device'];
    
    % Walk through the devices and find the slave with specified 
    % ProductCode and RevisionNo. If RevisionNo is empty, the select the
    % last device that is not overwritten by others using the 
    % values in HideType
    hide = [];
    slaveElem = [];
    name = '';
    for i = 0:device.getLength-1
        try
            % ProductCode is an attribute of element <Type>
            s = device.item(i);
            type = s.getElementsByTagName('Type').item(0);
            pc = fromHexString(type.getAttribute('ProductCode'));
            if ProductCode ~=  pc
                continue
            end
            
            r = fromHexString(type.getAttribute('RevisionNo'));
            if isempty(RevisionNo)
                % No RevisionNo was specified, so look for the latest
                
                % Save all RevisionNo from the <HideType> elements
                % in a list
                hideElem = s.getElementsByTagName('HideType');
                for j = 0:hideElem.getLength-1
                    str = hideElem.item(j).getAttribute('RevisionNo');
                    if length(str)
                        hide(length(hide)+1) = fromHexString(str);
                    end
                end
                
                % Next device if this one is in the hide list
                if intersect(hide,r)
                    % This revision is overwritten
                    continue
                end
                
                % This is the latest device at the moment
                name = char(type.getTextContent.trim);
                slaveElem = s;
                
            else
                % RevisionNo is specified, so check it and end the loop
                % if the device matches
                if r == RevisionNo
                    name = char(type.getTextContent.trim);
                    slaveElem = s;
                    break
                end
            end
        catch
            % nothing
        end
    end

    if isempty(slaveElem)
        error('getSlaveInfo:getSlaveInfo:noDevice', ...
            ['XML Document does not have device #x' dec2hex(ProductCode)]);
    end

    TxPdo = getPdo(slaveElem, parent, 'TxPdo', PdoList);
    RxPdo = getPdo(slaveElem, parent, 'RxPdo', PdoList);

% Parses slave xml element for the pdo and returns a structure
% vector containing:
% index
% subindex
% bitlen
% signed
function pdo = getPdo(slave, parent, dir, PdoList)
    pdos = slave.getElementsByTagName(dir);
    if ~pdos.getLength
        pdo = [];
        return
    end

    parent = [parent '/' dir];
    
    % Preassign pdo
    pdo = [];

    pdoIndexList = [];
    true = java.lang.String('1');
    
    for i = 0:pdos.getLength-1
        p = pdos.item(i);
        index = p.getElementsByTagName('Index').item(0);
        if ~size(index)
            error('getSlaveInfo:getPDO:elementNotFound',...
                'XML element <%s> does not have child <Index>.', parent);
        end
        index = fromHexString(index.getTextContent.trim);
        mandatory = p.getAttribute('Mandatory').equals(true);

        % Skip this PDO if it does not appear in the supplied PdoList
        if ~isempty(PdoList) && isempty(intersect(PdoList,index))
            if mandatory
                error(['PDO #x' dec2hex(index) ' is mandatory']);
            end
            continue
        end

        % Make sure that PDO's that are already mapped are do clash with 
        % this one
        exclude = p.getElementsByTagName('Exclude');
        skip = 0;
        x = 0;
        for j = 0:exclude.getLength-1
            x = fromHexString(exclude.item(j).getTextContent.trim);
            if intersect(pdoIndexList, x)
                skip = 1;
                break;
            end
        end
        if skip
            % upps, a PDO that is already mapped is excluded by this one
            if mandatory
                error(['PDO #x' dec2hex(index) ' is mandatory, '...
                    'but is excluded by #x' dec2hex(x)]);
            end
            continue
        end
        
        pdoIdx = size(pdo,2)+1;
        pdoIndexList(pdoIdx) = index;
        pdo(pdoIdx).index = index;
        
        % Save the PDO name
        name = p.getElementsByTagName('Name').item(0);
        if name.getLength
            pdo(pdoIdx).name = char(name.getTextContent.trim);
        else
            pdo(pdoIdx).name = '';
        end
        
        % Read all of the PDO Entries into field 'entry'
        entries = p.getElementsByTagName('Entry');
        pdo(pdoIdx).entry = repmat( ...
            struct('index',0,'subindex',0,'bitlen',0,'datatype',0), ...
            1, entries.getLength);
        for j = 1:entries.getLength
            pdo(pdoIdx).entry(j) = ...
                getPdoEntry(pdo(pdoIdx).entry(j), entries.item(j-1), parent);
        end
    end
        
function PdoEntry = getPdoEntry(PdoEntry, entry, parent)
    index = [];
    subindex = [];
    bitlen = [];
    datatype = [];
    try
        index    = entry.getElementsByTagName('Index').item(0);
        bitlen   = entry.getElementsByTagName('BitLen').item(0);
        subindex = entry.getElementsByTagName('SubIndex').item(0);
        datatype = entry.getElementsByTagName('DataType').item(0);
    catch
    end

    % Index Element is required
    if ~size(index)
        error('getSlaveInfo:getPDO:elementNotFound',...
            'XML element <%s/Entry> does not have child <Index>.', ...
            parent);
    elseif ~size(bitlen)
        error('getSlaveInfo:getPDO:elementNotFound',...
            'XML element <%s/Entry> does not have child <BitLen>.', ...
            parent);
    end

    PdoEntry.index  = fromHexString(index.getTextContent.trim);
    PdoEntry.bitlen = fromHexString(bitlen.getTextContent.trim);

    if ~PdoEntry.bitlen
        error('getSlaveInfo:getPDO:zeroLen',...
            'BitLen cannot be zero');
    end

    if ~PdoEntry.index
        return
    end

    % Index is specified, Subindex is mandatory
    if ~size(subindex)
        error('getSlaveInfo:getPDO:elementNotFound',...
            'XML element <%s/Entry> does not have child <SubIndex>.', ...
            parent);
    end
    PdoEntry.subindex = fromHexString(subindex.getTextContent.trim);

    if ~size(datatype)
        error('getSlaveInfo:getPDO:elementNotFound',...
            'XML element <%s/Entry> does not have child <DataType>.', ...
            parent);
    end

    PdoEntry.datatype = getDataType(datatype.getTextContent.trim,PdoEntry.bitlen);

% Returns whether a data type is signed
function w = getDataType(dt,bitlen)
    if dt.startsWith('BOOL')
        w = 1;
    elseif dt.startsWith('USINT') || dt.startsWith('UINT8') ...
            || dt.startsWith('BYTE') || dt.startsWith('STRING')
        w = 8;
    elseif dt.equals('UINT') || dt.startsWith('UINT16')
        w = 16;
    elseif dt.startsWith('UDINT') || dt.startsWith('UINT32')
        w = 32;
    elseif dt.startsWith('ULINT') || dt.startsWith('UINT64')
        w = 64;
    elseif dt.startsWith('SINT') || dt.startsWith('INT8')
        w = -8;
    elseif dt.equals('INT') || dt.startsWith('INT16')
        w = -16;
    elseif dt.startsWith('DINT') || dt.startsWith('INT32')
        w = -32;
    elseif dt.startsWith('LINT') || dt.startsWith('INT64')
        w = -64;
    elseif dt.startsWith('BIT')
        w = bitlen;
    else
        error(['Unknown data type ' char(dt)]);
    end

% This function converts hexadecimal values of the form
% '#x1234' to decimal values
function x = fromHexString(v)
    if v.startsWith('#x')
        x = hex2dec(char(v.substring(2,v.length)));
    else
        x = str2double(v);
    end
    if isnan(x)
        error('getSlaveInfo:getPDO:NaN', ...
            ['Value "' char(v) '" does not represent a number']);
    end
