function pdo_entry = XML_ParsePdoEntry(xml)

pdo_entry.Index = [];
pdo_entry.SubIndex = [];
pdo_entry.BitLen = [];
pdo_entry.DataType = [];

if ~nargin
    return
end

%% Get the Index and BitLen - required
try
    index = xml.getElementsByTagName('Index').item(0);
    pdo_entry.Index = fromHexString(index.getTextContent.trim);
    bit_len = xml.getElementsByTagName('BitLen').item(0);
    pdo_entry.BitLen= fromHexString(bit_len.getTextContent.trim);
catch
end

if isempty(pdo_entry.Index) || isempty(pdo_entry.BitLen)
    invalidDocument
end

if ~pdo_entry.Index
    return
end

%% Parse SubIndex and DataType

try
    subindex = xml.getElementsByTagName('SubIndex').item(0);
    pdo_entry.SubIndex = fromHexString(subindex.getTextContent.trim);
    data_type = xml.getElementsByTagName('DataType').item(0);
    data_type = data_type.getTextContent.trim;
catch
    invalidDocument
end

pdo_entry.DataType = getDataType(data_type, pdo_entry.BitLen);

%%
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
    error('Unknown data type %s', char(dt))
end

    

%%
function invalidDocument
error('XML file is not a valid EtherCATInfo Document');
