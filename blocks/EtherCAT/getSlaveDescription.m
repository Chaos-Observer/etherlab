function slave = getSlaveInfo(xmlfile, productCode, revision)

    tree = xmlread(xmlfile);

    info = tree.getDocumentElement;

    if ~strcmp(info.getNodeName, 'EtherCATInfo')
        error('getSlaveInfo:getSlaveInfo:elementNotFound',...
                'XML Document does not have root element <EtherCATInfo>');
    end

    parent = '/EtherCATInfo';

    try
        descriptions = info.getElementsByTagName('Descriptions');
        devices = descriptions.item(0).getElementsByTagName('Devices');
        device = devices.item(0).getElementsByTagName('Device');
        if ~device.getLength
            error(' ')
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
    slave = struct();

    for i = 0:device.getLength-1
        s = device.item(i);
        type = s.getElementsByTagName('Type');
        if ~type.getLength
            continue
        end
        type = type.item(0);
        c = fromHexString(type.getAttribute('ProductCode'));
        r = fromHexString(type.getAttribute('RevisionNo'));

        if productCode ~=  c || revision ~= r
            continue
        end

        slave.product_code = productCode;
        slave.revision = revision;
        slave.TxPdo = getPdo(s, parent, 'TxPdo');
        slave.RxPdo = getPdo(s, parent, 'RxPdo');

        break
    end

% Parses slave xml element for the pdo and returns a structure
% vector containing:
% index
% subindex
% bitlen
% signed
function pdo = getPdo(slave, parent, dir)
    pdos = slave.getElementsByTagName(dir);
    if ~pdos.getLength
        pdo = [];
        return
    end

    parent = [parent '/' dir];
    
    % Preassign pdo
    pdo(pdos.getLength) = struct('index',0,'entry',struct());

    for i = 1:pdos.getLength
        p = pdos.item(i-1);
        index = p.getElementsByTagName('Index').item(0);
        if ~size(index)
            error('getSlaveInfo:getPDO:elementNotFound',...
                'XML element <%s> does not have child <Index>.', parent);
        end
        pdo(i).index = fromHexString(index.getTextContent.trim);

        entries = p.getElementsByTagName('Entry');
        if ~entries.getLength
            continue
        end

        pdo(i).entry = repmat(...
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
            end
            
            pdo(i).entry(j).index = fromHexString(index.getTextContent.trim);
            pdo(i).entry(j).subindex = 0;
            if size(bitlen)
                pdo(i).entry(j).bitlen = fromHexString(bitlen.getTextContent.trim);
            end
            
            if ~pdo(i).entry(j).index
                % Empty node if index == 0
                continue
            end

            if ~size(subindex)
                error('getSlaveInfo:getPDO:elementNotFound',...
                    'XML element <%s/Entry> does not have child <SubIndex>.', ...
                    parent);
            elseif ~size(bitlen)
                error('getSlaveInfo:getPDO:elementNotFound',...
                    'XML element <%s/Entry> does not have child <BitLen>.', ...
                    parent);
            elseif ~size(datatype)
                error('getSlaveInfo:getPDO:elementNotFound',...
                    'XML element <%s/Entry> does not have child <DataType>.', ...
                    parent);
            end
            pdo(i).entry(j).subindex = fromHexString(subindex.getTextContent.trim);
            pdo(i).entry(j).signed = isSigned(datatype.getTextContent.trim);
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