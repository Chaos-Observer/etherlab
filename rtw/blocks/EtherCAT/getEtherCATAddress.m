function [Address masterStr positionStr] = ...
    getEtherCATAddress(block,masterArg,indexArg)
% This function takes the arguments masterAg and indexArg to 
% calculate the values for master, domain, alias, position
%
% master and domain are derived from masterArg. 
% If masterArg is a vector with at least 2 elements, 
%   master = masterArg(1);
%   domain = masterArg(2);
% otherwise
%   master = masterArg(1);
%   domain = 0;
%
% alias and position are derived from indexArg
% If indexArg is a vector with at least 2 elements
%   alias = indexArg(1)
%   position = indexArg(2)
% otherwise
%   alias = 0
%   position = indexArg(1)

Address = struct('master',0,'domain',0,'alias',0,'position',0);

if ~isnumeric(masterArg) || ~isnumeric(indexArg)
    error([block ': master and index must be numeric']);
end

% Calculate master and domain id.
% accept 2 forms for master:
% Scalar: master = value
% else Vector: [master domain]
Address.master = masterArg(1);
if sum(size(masterArg)) > 2
    Address.domain = masterArg(2);
end

if Address.domain
    masterStr = sprintf('%u.%u', Address.master, Address.domain);
else
    masterStr = sprintf('%u', Address.master);
end

% Calculate slave alias and position, and format positionStr for
% display in the block graphics
if sum(size(indexArg)) > 2
    Address.alias = indexArg(1);
    Address.position = indexArg(2);
else
    Address.position = indexArg(1);
end

if Address.alias
    positionStr = sprintf('#%u:%u', Address.alias, Address.position);
else
    positionStr = num2str(Address.position);
end

