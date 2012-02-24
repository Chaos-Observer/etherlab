function val = readNumericValue(node,nanAllowed,attribute)
% NumericValue: val = readNumericValue(node,nanAllowed,attr)
% Returns the numeric value of xml node, or of the attribute
% if it is supplied.

if nargin > 2
    s = node.getAttribute(attribute);
else
    s = node.getTextContent.trim;
end

val = str2double(s);

if isnan(val) && ~nanAllowed
    InvalidDoc(node,'readNumericValue','InvalidNumericValue');
end
