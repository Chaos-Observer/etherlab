function pdo = XML_ParsePdo(xml)
 
pdo.Sm = [];
pdo.Su = [];
pdo.Index = [];
pdo.Exclude = [];
pdo.Mandatory = 0;
pdo.OSIndexInc = [];
pdo.Entry = struct;
pdo.Virtual = 0;
if ~nargin
    return
end

%% Get the attributes
pdo.Mandatory = readBooleanValue(xml,'Mandatory');
pdo.Sm = readNumericValue(xml,1,'Sm');
pdo.Su = readNumericValue(xml,1,'Su');
pdo.OSIndexInc = readNumericValue(xml,1,'OSIndexInc');
pdo.Virtual = readBooleanValue(xml,'Virtual');

%% Try to get the Index
index = xml.getElementsByTagName('Index').item(0);
if isempty(index)
    InvalidDoc(xml,'XML_ParsePdo','ElementNotFound','Index');
end

pdo.Index = readHexDecValue(index,0);

%% Exclusion list
exclude = xml.getElementsByTagName('Exclude');
pdo.Exclude = repmat(0,1,exclude.getLength);
for i = 1:exclude.getLength
    pdo.Exclude(i) = readHexDecValue(exclude.item(i-1),1);
end

%% Get the Entries

entry = xml.getElementsByTagName('Entry');
pdo.Entry = repmat(XML_ParsePdoEntry,1,entry.getLength);
for i = 1:entry.getLength
    pdo.Entry(i) = XML_ParsePdoEntry(entry.item(i-1));
end
