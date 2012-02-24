function val = readBooleanValue(node,attribute)
% readBooleanValue: val = readBooleanValue(node,attr)

if nargin > 1
    s = node.getAttribute(attribute);
else
    s = node.getTextContent.trim;
end

val = s.equalsIgnoreCase('true') || s.equals(java.lang.String('1'));
