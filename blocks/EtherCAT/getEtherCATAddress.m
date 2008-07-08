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

Address = struct('Master',0,'Domain',0,'Alias',0,'Position',0);

if ~isnumeric(masterArg) || ~isnumeric(indexArg)
    error([block ': master and index must be numeric']);
end

% Calculate master and domain id.
% accept 2 forms for master:
% Scalar: master = value
% else Vector: [master domain]
Address.Master = masterArg(1);
if sum(size(masterArg)) > 2
    Address.Domain = masterArg(2);
end

if Address.Domain
    masterStr = sprintf('%u.%u', Address.Master, Address.Domain);
else
    masterStr = sprintf('%u', Address.Master);
end

% Calculate slave alias and position, and format positionStr for
% display in the block graphics
if sum(size(indexArg)) > 2
    Address.Alias = indexArg(1);
    Address.Position = indexArg(2);
else
    Address.Position = indexArg(1);
end

if Address.Alias
    positionStr = sprintf('#%u:%u', Address.Alias, Address.Position);
else
    positionStr = num2str(Address.Position);
end

