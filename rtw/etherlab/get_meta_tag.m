function rv = get_meta_tag(s)
% This helper function finds the first match of the form
% <meta .*/> in a string. It is called by get_description.tlc
% because it cannot do regex.
%
% Copyright (c) 2006 Richard Hacker
% License: GPL
%

m = regexp(s,'<meta.+?/>','match');
m = strrep(m, '\\', '');
m = strrep(m, '\"', '"');
m = strrep(m, '\n', '');
if size(m)
  rv = m{1};
else
  rv = '';
end
return
