function EtherCATInfo =  ...
        getEtherCATInfo(varargin)
    % EtherCATInfo =  ...
    %   getEtherCATInfo(xmlfile, ProductCode, RevisionNo, PdoList)
    %
    % Arguments:
    %   xmlfile (required): string pointing to EtherCATInfo xml file
    %   ProductCode (optional): Product code to find; Use first
    %       product if unspecified or empty
    %   RevisionNo (optional): if specified, only the slave device
    %       with the specified revision is considered. If empty,
    %       the last slave device that is not hidden by other slaves
    %       is returned.
    % 
    % Returns:
    % structure containing:
    %   .VendorId                   : number
    %   .RevisionNo                 : number
    %   .ProductCode                : number
    %   .Type                       : string
    %   .Sm.Read                    : Syncmanagers available for data input
    %      .Write                   : Syncmanagers available for data output
    %   .RxPdo().                   : see TxPdo
    %   .TxPdo().Index              : number
    %           .Name               : string
    %           .Exclude            : List of Pdo Indice's to exclude
    %           .Sm                 : Default Syncmanager (empty if not
    %                                 specified)
    %           .Mandatory          : must be included
    %           .Entry().Index      : number
    %                   .SubIndex   : number
    %                   .BitLen     : number of bits in Pdo Entry
    %                   .Name       : string
    %                   .DataType   : one of 1, -8, 8, -16, 16, -32, 32

    EtherCATInfo = struct('VendorId',[],'Device',[]);
    
    argc = length(varargin);
    
    if ~argc
        return
    end
    
    xmlfile = varargin{1};

    if ~ischar(xmlfile)
        error('First argument must be a string file name');
    end
    
    global debug
    debug = 0;
    args = struct('ProductCode',[],'RevisionNo',[]);
    
    for i = 1:argc/2
        field = varargin{i*2};
        switch lower(field)
            case 'productcode'
                args.ProductCode = varargin{i*2+1};
            case 'revisionno'
                args.RevisionNo = varargin{i*2+1};
            case 'debug'
                debug = varargin{i*2+1}(1);
                if ~isnumeric(debug)
                    error('Value of debug argument must be numeric.');
                end
        end
    end

    tree = xmlread(xmlfile);
    info = tree.getDocumentElement;

    if ~strcmp(info.getNodeName, 'EtherCATInfo')
        error('getEtherCATInfo:getEtherCATInfo:elementNotFound',...
                'XML Document does not have root element <EtherCATInfo>');
    end

    parent = '/EtherCATInfo';
    
    VendorElem = [];
    VendorIdTxt = [];
    try
        VendorElem = info.getElementsByTagName('Vendor').item(0);
        VendorIdTxt = VendorElem.getElementsByTagName('Id').item(0);
        EtherCATInfo.VendorId = fromHexString(VendorIdTxt.getTextContent.trim);
    catch
        if isempty(VendorElem)
            elem = 'Vendor';
        elseif isempty(VendorIdTxt)
            elem = 'Vendor/Id';
        end
        error('getEtherCATInfo:getEtherCATInfo:elementNotFound',...
            'XML Element %s/%s does not exist.', parent, elem);
    end
    
    if isempty(EtherCATInfo.VendorId)
        fprintf('XML Element content %s/Vendor/Id is not a valid number.',...
            parent);
        return
    end

    if debug >= 2
        fprintf('Found VendorId = %u\n', EtherCATInfo.VendorId);
    end
    
    descriptionsElem = [];
    devicesElem = [];
    deviceElem = [];
    try
        descriptionsElem = info.getElementsByTagName('Descriptions').item(0);
        devicesElem = descriptionsElem.getElementsByTagName('Devices').item(0);
        deviceElem = devicesElem.getElementsByTagName('Device');
    catch
        if isempty(descriptionsElem)
            elem = 'Descriptions';
        elseif isempty(devicesElem)
            elem = 'Descriptions/Devices';
        else
            elem = 'Descriptions/Devices/Device';
        end
        error('getEtherCATInfo:getEtherCATInfo:elementNotFound',...
            'XML Element %s/%s does not exist.', parent, elem);
    end

    parent = [parent '/Descriptions/Devices'];
    
    if ~deviceElem.getLength
        if debug
            fprintf('XML Document path %s does not have any <Device> child elements.',...
                parent);
        end
        return
    end

    parent = [parent '/Device('];
    
    % Walk through the devices and find the slave with specified 
    % ProductCode and RevisionNo. If RevisionNo is empty, the select the
    % last device that is not overwritten by others using the 
    % values in HideType
    slaveIdx = 1;
    for slaveElemIdx = 0:deviceElem.getLength-1
        DevicePath = [parent num2str(slaveElemIdx) ')'];

        device = struct(...
            'Type','',...
            'ProductCode',[],...
            'RevisionNo',[],...
            'Name','',...
            'HideType',[],...
            'Sm',[],...
            'TxPdo',[],...
            'RxPdo',[]);
        
        slaveElem = deviceElem.item(slaveElemIdx);
        try
            % ProductCode is an attribute of element <Type>
            type = slaveElem.getElementsByTagName('Type').item(0);
            device.Type = char(type.getTextContent.trim);
            device.ProductCode = fromHexString(type.getAttribute('ProductCode'));
            device.RevisionNo = fromHexString(type.getAttribute('RevisionNo'));
        catch
            if isempty(type) || isempty(device.Type)
                reject_no_text_element(DevicePath,'Type');
                continue
            end
        end

        attr = [];
        if isempty(device.ProductCode)
            attr = 'ProductCode';
        elseif isempty(device.RevisionNo);
            attr = 'RevisionNo';
        end
        if ~isempty(attr)
            reject_no_attribute(DevicePath,attr);
            continue
        end
        
        if isempty(device.Type)
            device.Type = ['#x' dec2hex(device.ProductCode)];
            if debug > 1
                fprintf('Device %s/Type has no text content. Presetting to %s\n',...
                    DevicePath, device.Type);
            end
        end
        
        if debug
            fprintf('Found Device(%u): Type %s, ProductCode #x%08x, Revision #x%08x...',...
                slaveElemIdx, device.Type, device.ProductCode, device.RevisionNo);
        end

        if ~ismatch(args, device.ProductCode, device.RevisionNo)
            if debug 
                fprintf(' not selected\n');
            end
            continue
        end
        if debug
            fprintf(' adding to list\n');
        end

        if isstruct(EtherCATInfo.Device)
            EtherCATInfo.Device(slaveIdx) = GetDevice(device,slaveElem,DevicePath);
        else
            EtherCATInfo.Device = GetDevice(device,slaveElem,DevicePath);
        end
        slaveIdx = slaveIdx + 1;
    end
end

function  reject_no_element(DevicePath,type)
    global debug
    if debug
        fprintf('XML element %s/Type does not exist; Rejecting...\n', ...
            DevicePath,type);
    end
end

function  reject_no_text_element(DevicePath,element)
    global debug
    if debug
        fprintf(['XML path %s does not have a text child element <%s>. '...
            'Rejecting...\n'], DevicePath, element);
    end
end

function  reject_no_numeric_element(Path,element)
    global debug
    if debug
        fprintf(['XML path %s does not have a valid numeric child element <%s>. '...
            'Rejecting...\n'], Path, element);
    end
end

function reject_no_attribute(DevicePath,attr)
    global debug
    if debug
        fprintf(['XML element %s/Type '...
            'does not have numeric attribute %s; Rejecting... \n'], ...
            DevicePath,attr);
    end
end

function x = ismatch(args,ProductCode,RevisionNo)
    %% Returns true when:
    %%    - args.ProductCode is missing
    %%    - or ProductCode matches and args.RevisionNo is missing
    %%    - or RevisionNo matches
    %% otherwise false
    %%
    %% The idea is that if a property is not set, it matches
    %% 
    if isempty(args.ProductCode) ...
            || isequal(args.ProductCode,ProductCode) && isempty(args.RevisionNo) ...
            || isequal(args.RevisionNo,RevisionNo)
        x = true;
    else
        x = false;
    end
end

function device = GetDevice(device,slaveElem,DevicePath)
    global debug
    
    if debug > 1
        fprintf('Parsing Device at %s...\n', DevicePath);
    end
    
    % Save all RevisionNo from the <HideType> elements
    % in a list
    element = slaveElem.getElementsByTagName('HideType');
    if ~element.getLength && debug > 1
        fprintf('Slave does not have any children <HideType>\n');
    end
    for i = 0:element.getLength-1
        h = fromHexString(element.item(i).getAttribute('RevisionNo'));
        device.HideType = [device.HideType h];
        if isempty(h)
            hidepath = [DevicePath '/HideType(' num2str(i) ')'];
            reject_no_attribute(hidepath,'RevisionNo');
        end
    end
    if debug
        fprintf('Hiding RevisionNo');
        for i = 1:length(device.HideType)
            fprintf(' #x%08x', device.HideType(i));
        end
        fprintf('\n');
    end
    
    device.Sm = struct('Input',[],'Output',[]);
    element = slaveElem.getElementsByTagName('Sm');
    for SmIdx = 0:element.getLength-1
        str = element.item(SmIdx).getAttribute('ControlByte');
        ControlByte = uint8(fromHexString(str));
        if isempty(ControlByte)
            hidepath = [DevicePath '/Sm(' num2str(SmIdx) ')'];
            reject_no_attribute(hidepath,'ControlByte');
            continue
        end
        
        if debug > 1
            fprintf('Sm(%u) has ControlByte #x%x... ', SmIdx, ControlByte);
        end
        
        % (Bit1,Bit0) of ControlByte must equal (0,0)
        if bitand(ControlByte,3)
            if debug > 1
                fprintf('no IO SyncManager; continue\n');
            end
            continue
        end
        if debug > 1
            fprintf('\n');
        end
        
        % (Bit3,Bit2) is the direction:
        %   (0,0)
        %   (0,1)
        switch bitand(bitshift(ControlByte,-2),3)
            case 0
                device.Sm.Input = [device.Sm.Input SmIdx];
            case 1
                device.Sm.Output = [device.Sm.Output SmIdx];
            otherwise
                if debug
                    fprintf(['Unknown direction bits in ControlByte '...
                        'of XML element %s/Sm(%u).'], DevicePath, SmIdx);
                end
        end
    end
    if debug
        fprintf('Available SyncManagers: ');
        if length(device.Sm.Input)
            fprintf('Input = %u', device.Sm.Input(1))
            for i = 2:length(device.Sm.Input)
                fprintf(', %u', device.Sm.Input(i))
            end
            fprintf('; ')
        end
        if length(device.Sm.Output)
            fprintf('Output = %u', device.Sm.Output(1))
            for i = 2:length(device.Sm.Output)
                fprintf(', %u', device.Sm.Output(i))
            end
        end
        fprintf('\n')
    end
    
    
    % Get the list of slaves assigned to the SyncManagers
    skelPdo = struct(...
        'Index',[],...
        'Sm',[],...
        'Mandatory',[],...
        'Name','',...
        'Exclude',[],...
        'Entry',[]);
    
    TxPdoElem = slaveElem.getElementsByTagName('TxPdo');
    device.TxPdo = repmat(skelPdo, 1, TxPdoElem.getLength);
    for i = 1:TxPdoElem.getLength
        device.TxPdo(i) = getPdo(device.TxPdo(i), TxPdoElem.item(i-1),...
            [DevicePath '/TxPdo(' num2str(i-1) ')']);
    end
%     device.RxPdo = getPdos(slaveElem, parent, 'RxPdo');
    
end

% Parses slave xml element for the pdo and returns a structure
% vector containing:
% index
% subindex
% bitlen
% signed
function pdo = getPdo(pdo, pdoElement, PdoPath)
    global debug
    
    if debug > 1
        fprintf('Parsing pdos in %s\n', PdoPath);
    end

    try
        indexElem = pdoElement.getElementsByTagName('Index').item(0);
        pdo.Index = fromHexString(indexElem.getTextContent.trim);
        pdo.Mandatory = fromHexString(indexElem.getAttribute('Mandatory'));
        pdo.Sm = fromHexString(indexElem.getAttribute('Sm'));
    catch
    end
    
    if isempty(pdo.Index)
        reject_no_numeric_element(PdoPath, 'Index');
        return
    end
    
    if isempty(pdo.Mandatory)
        pdo.Mandatory = 0;
    end
    if isempty(pdo.Sm)
        pdo.Sm = -1;
    end
    
    if debug
        fprintf('Found Pdo Index #x%04x, Mandatory %u, SyncManager %i\n',...
            pdo.Index, pdo.Mandatory, pdo.Sm);
    end

    ExcludeElements = pdoElement.getElementsByTagName('Exclude');
    pdo.Exclude = zeros(1,ExcludeElements.getLength);
    for i = 1:ExcludeElements.getLength
        try
            x = ExcludeElements.item(i-1).getTextContent.trim;
            pdo.Exclude(i) = fromHexString(x);
        catch
            reject_no_numeric_element(PdoPath,['Exclude(' num2str(i) ')']);
        end
    end
    if debug
        fprintf('Exclude Pdo:');
        for i = 1:length(pdo.Exclude)
            fprintf(' #x%04x', pdo.Exclude(i));
        end
        fprintf('\n');
    end
    
    
    pdoEntryElem = pdoElement.getElementsByTagName('Entry');
    pdo.Entry = repmat(struct(...
            'Index',[],...
            'SubIndex',[],...
            'BitLen',[],...
            'DataType',[]), 1, pdoEntryElem.getLength);

    for i = 1:pdoEntryElem.getLength
        pdo.Entry(i) = getPdoEntry(pdo.Entry(i),pdoEntryElem.item(i-1),...
            [PdoPath '/Entry(' num2str(i-1) ')']);
    end
end
        
function PdoEntry = getPdoEntry(PdoEntry, PdoEntryElem, EntryPath)
    global debug
    
    copy = PdoEntry;
    
    if debug > 1
        fprintf('Parsing Pdo Entry %s.\n', EntryPath);
    end

    try
        index    = PdoEntryElem.getElementsByTagName('Index').item(0).getTextContent.trim;
        copy.Index = fromHexString(index);
        bitlen   = PdoEntryElem.getElementsByTagName('BitLen').item(0).getTextContent.trim;
        copy.BitLen = fromHexString(bitlen);
        subindex = PdoEntryElem.getElementsByTagName('SubIndex').item(0).getTextContent.trim;
        copy.SubIndex = fromHexString(subindex);
        datatype = PdoEntryElem.getElementsByTagName('DataType').item(0).getTextContent.trim;
        copy.DataType = getDataType(datatype,copy.BitLen);
        copy.DataType = getDataType(datatype,copy.BitLen);
    catch
%        error('stop')
    end

    % Index Element is required
    if isempty(copy.Index)
        reject_no_numeric_element(EntryPath, 'Index');
        return
    elseif isempty(copy.BitLen)
        reject_no_numeric_element(EntryPath, 'BitLen');
        return
    end
    
    if ~copy.Index
        return
    end

    % Index is specified, Subindex is mandatory
    if isempty(copy.SubIndex)
        reject_no_numeric_element(EntryPath, 'SubIndex');
        return
    end

    if isempty(copy.DataType)
        reject_no_text_element(EntryPath,'DataType');
        return
    end
    
    PdoEntry = copy;
end        

% Returns whether a data type is signed
function w = getDataType(dt,bitlen)
    global debug

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
        if debug
            fprintf('Unknown DataType: %s\n', char(dt));
        end
        w = [];
    end
end
    
% This function converts hexadecimal values of the form
% '#x1234' to decimal values
function x = fromHexString(v)
    try
        if v.startsWith('#x')
            x = hex2dec(char(v.substring(2,v.length)));
        else
            x = str2double(v);
        end
    catch
        x = [];
    end
    if isnan(x)
        x = [];
    end
end