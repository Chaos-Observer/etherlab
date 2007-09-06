function rv = get_meta_tag(s)
% This helper function finds the first match of the form
% <meta .*/> in a string. It is called by get_description.tlc
% because it cannot do regex.
%
% Copyright (c) 2006 Richard Hacker
% License: GPL
%
% $RCSfile: get_meta_tag.m,v $
% $Revision: 1.2 $
%
% $Log: get_meta_tag.m,v $
% Revision 1.2  2006/02/28 06:32:45  rich
% removed unnecessary output
%
% Revision 1.1  2006/02/21 00:29:52  rich
% Initial revision
%

m = regexp(s,'<meta.+?/>','match');
if size(m)
  rv = m{1};
else
  rv = '';
end
return
