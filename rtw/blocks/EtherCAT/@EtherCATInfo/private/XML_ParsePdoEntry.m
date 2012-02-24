function pdo_entry = XML_ParsePdoEntry(xml)

pdo_entry.Index = [];
pdo_entry.SubIndex = [];
pdo_entry.BitLen = [];
pdo_entry.DataType = [];

if ~nargin
    return
end

%% Get the Index and BitLen - required
index = xml.getElementsByTagName('Index').item(0);
if isempty(index)
    InvalidDoc(xml,'XML_ParsePdoEntry','ElementNotFound','Index');
end

bit_len = xml.getElementsByTagName('BitLen').item(0);
if isempty(bit_len)
    InvalidDoc(xml,'XML_ParsePdoEntry','ElementNotFound','BitLen');
end

pdo_entry.Index = readHexDecValue(index,0);

pdo_entry.BitLen= readHexDecValue(bit_len,0);

if ~pdo_entry.Index
    return
end

%% Parse SubIndex (optional, default = 0)

subindex = xml.getElementsByTagName('SubIndex').item(0);
if ~isempty(subindex)
    pdo_entry.SubIndex = readHexDecValue(subindex,0);
else
    pdo_entry.SubIndex = 0;
end

%% Parse DataType (mandatory)
data_type = xml.getElementsByTagName('DataType').item(0);
if isempty(data_type)
    InvalidDoc(xml,'XML_ParsePdoEntry','ElementNotFound','DataType');
end
dt = data_type.getTextContent.trim;

if ~dt.length
    InvalidDoc(data_type, 'XML_ParsePdoEntry', 'InvalidDataType', dt);
end

%%
% Parse the data type.
% Generally the number is coded as
%     0x01??       Unsigned
%     0x02??       Signed
%     0x03??       float
% where ?? = the number of significant bits
%
% For unsigned integers, bit nibbling is allowed from 1 to 7 bits
% For all others, only multiple of bytes are allowed
%
% 0x0101          boolean_T
% 0x0102..0x0107  Bit2..Bit7
% 0x0108          Unsigned8
% 0x0110          Unsigned16
% 0x0118          Unsigned24
% 0x0120          Unsigned32
% 0x0128          Unsigned40
% 0x0130          Unsigned48
% 0x0138          Unsigned56
% 0x0140          Unsigned64
% 0x0208          Integer8
% 0x0210          Integer16
% 0x0218          Integer24
% 0x0220          Integer32
% 0x0228          Integer40
% 0x0230          Integer48
% 0x0238          Integer56
% 0x0240          Integer64
% 0x0320          Real32
% 0x0340          Real64

unsigned = uint16(hex2dec('100'));
signed   = uint16(hex2dec('200'));
float    = uint16(hex2dec('300'));

if dt.equals('REAL')
    dt_mod = dt.concat(int2str(pdo_entry.BitLen));
else
    dt_mod = dt;
end

switch char(dt_mod)

    % Boolean
    case 'BOOL'
        pdo_entry.DataType = unsigned + 1;

    % Signed integers
    case {'SINT', 'INT8'}
        pdo_entry.DataType = signed + 8;
    case {'INT', 'INT16'}
        pdo_entry.DataType = signed + 16;
    case {'DINT', 'INT32'}
        pdo_entry.DataType = signed + 32;
    case {'LINT', 'INT64'}
        pdo_entry.DataType = signed + 64;
    
    % Unsigned integers
    case {'USINT', 'UINT8', 'BYTE'}
        pdo_entry.DataType = unsigned + 8;
    case {'UINT', 'UINT16'}
        pdo_entry.DataType = unsigned + 16;
    case {'UDINT', 'UINT32', 'DWORD'}
        pdo_entry.DataType = unsigned + 32;
    case {'ULINT', 'UINT64'}
        pdo_entry.DataType = unsigned + 64;
    
    % Floating point
    case {'FLOAT', 'REAL32'}
        pdo_entry.DataType = float + 32;
    case {'DOUBLE', 'REAL64'}
        pdo_entry.DataType = float + 64;
    
    otherwise
        % Strings
        if dt.startsWith('STRING')
            pdo_entry.DataType = unsigned + 8;
        
        % Irregular integers
        elseif dt.startsWith('UINT') || dt.startsWith('BIT')
            if dt.startsWith('UINT')
                bits = str2double(dt.substring(4));
            else
                bits = str2double(dt.substring(3));
            end
            if ~bits || (bits > 7 && mod(bits,8))
                InvalidDoc(data_type, 'XML_ParsePdoEntry', ...
                            'InvalidDataType', dt);
            end
            pdo_entry.DataType = unsigned + bits;
        elseif dt.startsWith('INT')
            bits = str2double(dt.substring(3));
            if isempty(find([8 16 32 64] == bits, 1))
                InvalidDoc(data_type, 'XML_ParsePdoEntry', ...
                            'InvalidDataType', dt);
            end
            pdo_entry.DataType = signed + bits;
        
        else
            InvalidDoc(data_type, 'XML_ParsePdoEntry', ...
                        'InvalidDataType', dt);
        end
end
