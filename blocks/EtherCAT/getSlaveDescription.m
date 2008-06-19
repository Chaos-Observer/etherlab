function [vendorId name revision TxPdo RxPdo] = ...
        getSlaveDescription(xmlfile, productCode, revision,requestPdo)

    tree = xmlread(xmlfile);

    info = tree.getDocumentElement;

    if ~strcmp(info.getNodeName, 'EtherCATInfo')
        error('getSlaveInfo:getSlaveInfo:elementNotFound',...
                'XML Document does not have root element <EtherCATInfo>');
    end

    parent = '/EtherCATInfo';

    try
        vendor = info.getElementsByTagName('Vendor').item(0);
        vendorId = vendor.getElementsByTagName('Id').item(0);
        vendorId = fromHexString(vendorId.getTextContent.trim);
        descriptions = info.getElementsByTagName('Descriptions');
        devices = descriptions.item(0).getElementsByTagName('Devices');
        device = devices.item(0).getElementsByTagName('Device');
        if ~device.getLength
            error(' '); %Generate error, is caught below
        end
    catch
        p = '';
        c = '';
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

        error('getSlaveInfo:getSlaveInfo:elementNotFound',...
            'XML element <%s> does not have child <%s>.', p,c);
    end

    parent = [parent '/Descriptions/Devices/Device'];
    
    hide = [];
    slaveElem = [];
    findRevision = revision;
    name = '';
    for i = 0:device.getLength-1
        try
            s = device.item(i);
            type = s.getElementsByTagName('Type').item(0);
            pc = fromHexString(type.getAttribute('ProductCode'));
            if productCode ~=  pc
                continue
            end
            name = char(type.getTextContent.trim);
            revision = fromHexString(type.getAttribute('RevisionNo'));
            if isempty(findRevision)
                if find(revision == hide,1)
                    continue
                end
                hideElem = s.getElementsByTagName('HideType');
                for j = 0:hideElem.getLength-1
                    str = hideElem.item(j).getAttribute('RevisionNo');
                    if length(str)
                        hide(length(hide)+1) = fromHexString(str);
                    end
                end
                slaveElem = s;
            else
                if revision == findRevision
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
            ['XML Document does not have device #x' dec2hex(productCode)]);
    end

    TxPdo = getPdo(slaveElem, parent, 'TxPdo', requestPdo);
    RxPdo = getPdo(slaveElem, parent, 'RxPdo', requestPdo);

% Parses slave xml element for the pdo and returns a structure
% vector containing:
% index
% subindex
% bitlen
% signed
function pdo = getPdo(slave, parent, dir, requestPdo)
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

        if ~isempty(requestPdo) && isempty(find(index == requestPdo,1))
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
        
        name = p.getElementsByTagName('Name').item(0);
        if name.getLength
            pdo(pdoIdx).name = char(name.getTextContent.trim);
        else
            pdo(pdoIdx).name = '';
        end
        
        entries = p.getElementsByTagName('Entry');
        if ~entries.getLength
            pdo(pdoIdx).entry = [];
            continue
        end
        
        pdo(pdoIdx).entry = repmat(...
            struct('index',0,'subindex',0,'bitlen',0,'signed',0), ...
            1,entries.getLength);
        for j = 1:entries.getLength
            % Cache the XML Entry Element
            entry = entries.item(j-1);

            index = entry.getElementsByTagName('Index').item(0);
            subindex = entry.getElementsByTagName('SubIndex').item(0);
            bitlen = entry.getElementsByTagName('BitLen').item(0);
            datatype = entry.getElementsByTagName('DataType').item(0);

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
            
            pdo(pdoIdx).entry(j).index = fromHexString(index.getTextContent.trim);
            pdo(pdoIdx).entry(j).subindex = 0;
            pdo(pdoIdx).entry(j).bitlen = fromHexString(bitlen.getTextContent.trim);
            
            if ~pdo(pdoIdx).entry(j).index
                % Empty node if index == 0
                continue
            end

            if ~size(subindex)
                error('getSlaveInfo:getPDO:elementNotFound',...
                    'XML element <%s/Entry> does not have child <SubIndex>.', ...
                    parent);
            end
            pdo(pdoIdx).entry(j).subindex = ...
                fromHexString(subindex.getTextContent.trim);

            if ~pdo(pdoIdx).entry(j).bitlen
                continue
            end

            if ~size(datatype)
                error('getSlaveInfo:getPDO:elementNotFound',...
                    'XML element <%s/Entry> does not have child <DataType>.', ...
                    parent);
            end
            pdo(pdoIdx).entry(j).signed = isSigned(datatype.getTextContent.trim);
        end
    end


% Returns whether a data type is signed
function s = isSigned(dt)
    switch char(dt)
        case { 'SINT', 'INT', 'DINT', 'LINT', ...
                 'INT8', 'INT16', 'INT32', 'INT64'}
            s = 1;
        case {'USINT','UINT','UDINT','ULINT',...
                'UINT8','UINT16','UINT32','UINT64','BYTE', 'BOOL'}
            s = 0;
        otherwise
            error('Unknown data type %s.', char(dt));
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
