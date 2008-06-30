function EtherCATInfo =  ...
        getEtherCATInfo(xmlfile, ProductCode, RevisionNo, PdoList)
    % EtherCATInfo =  ...
    %   getEtherCATInfo(xmlfile, ProductCode, RevisionNo, PdoList)
    %
    % Arguments:
    %   xmlfile (required): string pointing to EtherCATInfo xml file
    %   ProductCode (required): Product code to find
    %   RevisionNo (optional): if specified, only the slave device
    %       with the specified revision is considered. If empty,
    %       the last slave device that is not hidden by other slaves
    %       is returned.
    %   PdoList (optional): If specified, only PDO's contained in
    %       the list are considered.
    % 
    % Returns:
    % structure containing:
    %   .VendorId                   : number
    %   .RevisionNo                 : number
    %   .ProductCode                : number
    %   .Type                       : string
    %   .RxPdo().                   : see TxPdo
    %   .TxPdo().Index              : number
    %           .Name               : string
    %           .Entry().Index      : number
    %                   .SubIndex   : number
    %                   .BitLen     : number of bits in Pdo Entry
    %                   .Name       : string
    %                   .DataType   : one of 1, -8, 8, -16, 16, -32, 32

    EtherCATInfo = struct(...
        'VendorId',0,'RevisionNo',0,'ProductCode',0,'Type','',...
        'SyncManager', {[]});

    if isempty(xmlfile)
        return
    end
    
    tree = xmlread(xmlfile);

    info = tree.getDocumentElement;

    if ~strcmp(info.getNodeName, 'EtherCATInfo')
        error('getEtherCATInfo:getEtherCATInfo:elementNotFound',...
                'XML Document does not have root element <EtherCATInfo>');
    end

    parent = '/EtherCATInfo';
    
    try
        Vendor = info.getElementsByTagName('Vendor').item(0);
        VendorIdTxt = Vendor.getElementsByTagName('Id').item(0);
        VendorId = fromHexString(VendorIdTxt.getTextContent.trim);
        descriptions = info.getElementsByTagName('Descriptions');
        devices = descriptions.item(0).getElementsByTagName('Devices');
        device = devices.item(0).getElementsByTagName('Device');
        if ~device.getLength
            %Generate empty error, is caught below
            error(' '); 
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

        error('getEtherCATInfo:getEtherCATInfo:elementNotFound',...
            'XML element <%s> does not have child <%s>.', p,c);
    end

    parent = [parent '/Descriptions/Devices/Device'];
    
    % Walk through the devices and find the slave with specified 
    % ProductCode and RevisionNo. If RevisionNo is empty, the select the
    % last device that is not overwritten by others using the 
    % values in HideType
    hide = [];
    slaveElem = [];
    Type = '';
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
                Type = char(type.getTextContent.trim);
                slaveElem = s;
                
            else
                % RevisionNo is specified, so check it and end the loop
                % if the device matches
                if r == RevisionNo
                    Type = char(type.getTextContent.trim);
                    slaveElem = s;
                    break
                end
            end
        catch
            % nothing
        end
    end

    if isempty(slaveElem)
        error('getEtherCATInfo:getEtherCATInfo:noDevice', ...
            ['XML Document does not have device #x' dec2hex(ProductCode) ...
            ' Revision #x' dec2hex(RevisionNo)]);
    end
    
    EtherCATInfo.VendorId = VendorId;
    EtherCATInfo.RevisionNo = RevisionNo;
    EtherCATInfo.RevisionNo = r;
    EtherCATInfo.ProductCode = ProductCode;
    EtherCATInfo.Type = Type;
    
    sm = slaveElem.getElementsByTagName('Sm');
    SyncManager = struct('Read',[],'Write',[]);
    read_idx = 1;
    write_idx = 1;
    for i = 0:sm.getLength-1
        smX = sm.item(i);
        if ~smX.hasAttribute('ControlByte')
            error('getEtherCATInfo:getEtherCATInfo:missingAttribute', ...
                'SyncManager element does not have attribute "ControlByte"');
        end
        ControlByte = uint8(fromHexString(smX.getAttribute('ControlByte')));
        if bitand(ControlByte,3)
            % Only looking for elements where (Bit1,Bit0) = (0,0)
            continue
        end
        direction = bitshift(bitand(ControlByte,12),-2);
        switch direction
            case 0
                SyncManager.Read(read_idx) = i;
                read_idx = read_idx + 1;
            case 1
                SyncManager.Write(write_idx) = i;
                write_idx = write_idx + 1;
            otherwise
                error('getEtherCATInfo:getEtherCATInfo:unknownSmDirection', ...
                    'Unknown SyncManager direction %u', direction);
        end
    end
    
    % Get the list of slaves assigned to the SyncManagers
    TxSm = getSmPdos(slaveElem, parent, 'TxPdo', PdoList);
    RxSm = getSmPdos(slaveElem, parent, 'RxPdo', PdoList);

    EtherCATInfo.SyncManager = {};
    sm_idx = 1;
    for i = 1:16
        Sm = {};
        dir = 0;
        if ~isempty(TxSm{i})
            dir = 1;
            Sm = TxSm{i};
        end
        if ~isempty(RxSm{i})
            if ~isempty(Sm)
                error('getEtherCATInfo:getEtherCATInfo:configError',...
                    'SyncManager %u can only have one direction.',i-1);
            end
            dir = 0;
            Sm = RxSm{i};
        end
        if isempty(Sm)
            continue
        end
        EtherCATInfo.SyncManager{sm_idx} = ...
            struct('SmIndex', i-1, 'SmDir', dir, 'Pdo', Sm);
        sm_idx = sm_idx + 1;
    end
end


% Parses slave xml element for the pdo and returns a structure
% vector containing:
% index
% subindex
% bitlen
% signed
function sm = getSmPdos(slave, parent, dir, PdoList)
    
    % Preassign pdo
    sm = cell(16,1);

    pdos = slave.getElementsByTagName(dir);
    if ~pdos.getLength
        return
    end

    parent = [parent '/' dir];
    
    pdoIndexList = [];
    true = java.lang.String('1');
    
    for i = 0:pdos.getLength-1
        pdo = struct();
        
        p = pdos.item(i);
        index = p.getElementsByTagName('Index').item(0);
        if ~size(index)
            error('getEtherCATInfo:getPDO:elementNotFound',...
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
        
        pdoIndexList(size(pdoIndexList,2)+1) = index;
        pdo.Index = index;
        Sm = str2double(p.getAttribute('Sm'));
        if isnan(Sm) || Sm < 0
            error('getEtherCATInfo:getPDO:attributeMissing',...
                'XML element <%s> does not required attribute "Sm"', ...
                parent);
        end
        
        % Save the PDO name
        name = p.getElementsByTagName('Name').item(0);
        if name.getLength
            pdo.Name = char(name.getTextContent.trim);
        else
            pdo.Name = '';
        end
        
        % Read all of the PDO Entries into field 'entry'
        entries = p.getElementsByTagName('Entry');
        pdo.Entry = repmat( ...
            struct('Index',0,'SubIndex',0,'BitLen',0,'DataType',0,'Name',''), ...
            1, entries.getLength);
        for j = 1:entries.getLength
            pdo.Entry(j) = ...
                getPdoEntry(pdo.Entry(j), entries.item(j-1), parent);
        end
        sm{Sm+1}(size(sm{Sm+1},2)+1) = pdo;
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
        name     = entry.getElementsByTagName('Name').item(0);
    catch
    end

    % Index Element is required
    if ~size(index)
        error('getEtherCATInfo:getPDO:elementNotFound',...
            'XML element <%s/Entry> does not have child <Index>.', ...
            parent);
    elseif ~size(bitlen)
        error('getEtherCATInfo:getPDO:elementNotFound',...
            'XML element <%s/Entry> does not have child <BitLen>.', ...
            parent);
    end

    PdoEntry.Index  = fromHexString(index.getTextContent.trim);
    PdoEntry.BitLen = fromHexString(bitlen.getTextContent.trim);

    if ~PdoEntry.BitLen
        error('getEtherCATInfo:getPDO:zeroLen',...
            'BitLen cannot be zero');
    end

    if ~PdoEntry.Index
        return
    end

    % Index is specified, Subindex is mandatory
    if ~size(subindex)
        error('getEtherCATInfo:getPDO:elementNotFound',...
            'XML element <%s/Entry> does not have child <SubIndex>.', ...
            parent);
    end
    PdoEntry.SubIndex = fromHexString(subindex.getTextContent.trim);

    if ~size(datatype)
        error('getEtherCATInfo:getPDO:elementNotFound',...
            'XML element <%s/Entry> does not have child <DataType>.', ...
            parent);
    end

    PdoEntry.DataType = getDataType(datatype.getTextContent.trim,PdoEntry.BitLen);
    
    % Save the PDO name
    if name.getLength
        PdoEntry.Name = char(name.getTextContent.trim);
    end
end        

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
        if bitlen == 1
            w = 1;
        else
            w = 8;
        end
    else
        error('getEtherCATInfo:getDataType:unknown',...
            ['Unknown data type ' char(dt)]);
    end
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
        error('getEtherCATInfo:getPDO:NaN', ...
            ['Value "' char(v) '" does not represent a number']);
    end
end