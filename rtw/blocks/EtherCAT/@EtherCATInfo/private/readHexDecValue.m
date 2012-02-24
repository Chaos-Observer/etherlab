function val = readHexDecValue(node,nanAllowed,attribute)
% readHexDecValue: val = readHexDecValue(node,nanAllowed,attr)
% Returns the numeric value of xml node, or of the attribute
% if it is supplied.
% If str starts with '#x', the following characters are
% interpreted as a hexadecimal value, otherwise it is assumed
% to be a decimal.

if nargin > 2
    s = node.getAttribute(attribute);
else
    s = node.getTextContent.trim;
end

val = NaN;

if s.startsWith('#x') && s.length > 2
    try
        val = hex2dec(char(s.substring(2)));
    catch
        InvalidDoc(node,'readHexDecValue','InvalidHexDecValue');
    end

elseif s.length
    val = str2double(s);
end

if isnan(val) && ~nanAllowed
    InvalidDoc(node,'readHexDecValue','InvalidHexDecValue');
end
