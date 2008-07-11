function pdo = XML_ParsePdo(xml)
 
pdo.Sm = [];
pdo.Index = [];
pdo.Exclude = [];
pdo.Mandatory = 0;
pdo.Entry = repmat(XML_ParsePdoEntry,1,0);
if ~nargin
    return
end

%% Get the attributes
pdo.Mandatory = double(strcmp(xml.getAttribute('Mandatory'),'1'));
pdo.Sm = fromHexString(xml.getAttribute('Sm'));

%% Try to get the Index
try
    index = xml.getElementsByTagName('Index').item(0);
    pdo.Index = fromHexString(index.getTextContent.trim);
catch
end

if isempty(pdo.Index)
    invalidDocument();
end

%% Exclusion list
exclude = xml.getElementsByTagName('Exclude');
for i = 1:exclude.getLength
    pdo.Exclude(i) = fromHexString(exclude.item(i-1).getTextContent.trim);
end

%% Get the Entries

entry = xml.getElementsByTagName('Entry');
for i = 1:entry.getLength
    pdo.Entry(i) = XML_ParsePdoEntry(entry.item(i-1));
end

%%
function invalidDocument
error('<Tx/RxPdo> element is invalid');
