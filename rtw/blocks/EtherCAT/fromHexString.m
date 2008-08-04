function val = fromHexString(v)
% fromHexString: val = fromHexString(str)
% Returns the numeric value of Java string 'str'. 
% If str starts with '#x', the following characters are
% interpreted as a hexadecimal value, otherwise it is assumed
% to be a decimal. [] is returned if there were any conversion
% exceptions
val = [];
try
    if v.startsWith('#x')
        val = hex2dec(char(v.substring(2,v.length)));
    else
        val = str2double(v);
    end
catch
    val = [];
end
if isnan(val)
    val = [];
end
