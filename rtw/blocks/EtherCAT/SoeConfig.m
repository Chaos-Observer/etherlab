function sc = SoeConfig(f)
% Parse an XML file containing the SoE configuration for a slave
% Returns a structure array with the elements
%    .Index             = <number>
%    .OctetString       = <string>

try
    ud = get_param(gcb, 'UserData');
    savedStat = ud.SoeConfigFile;
    stat = dir(f);
    if (savedStat.datenum == stat.datenum && savedStat.bytes == stat.bytes)
        sc = ud.SoeConfig;
        return
    end
catch
    disp(['No SoeConfig found in block ' gcb])
end

sc = repmat(struct('Index',[],'OctetString',''), 1, 0);

tree = xmlread(f);
root = tree.getDocumentElement;

if ~strcmp(root.getNodeName, 'SoeConfig')
    invalidDocument();
end

IdnElems = root.getElementsByTagName('IDN');

for i = 1:IdnElems.getLength
    sc(i) = XML_ParseIdn(IdnElems.item(i-1));
end

try
    ud = get_param(gcb, 'UserData');
    ud.SoeConfig = sc;
    ud.SoeConfigFile = dir(f);
    set_param(gcb,'UserData',ud);
catch
end

return

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
function idn = XML_ParseIdn(elem)
% Parse a single <IDN> element, containing the child elements:
% <IDN>
%    <Index>123</Index>    <!-- single number -->
%    <Data>1022317a</Data> <!-- Hex Numbers -->
% </IDN>

idn.Index = str2num(elem.getElementsByTagName('Index').item(0).getTextContent.trim);
idn.OctetString = '';

s = char(elem.getElementsByTagName('Data').item(0).getTextContent.trim);
for i = floor(length(s)/2):-1:1
    idn.OctetString(i) = char(hex2dec(s(i*2-1:i*2)));
end
