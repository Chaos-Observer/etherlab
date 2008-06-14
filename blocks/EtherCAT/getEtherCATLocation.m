function [master domain alias position masterStr positionStr] = ...
    getEtherCATLocation(block,masterArg,indexArg)
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

if ~isnumeric(masterArg) || ~isnumeric(indexArg)
    error([block ': master and index must be numeric']);
end

% Calculate master and domain id.
% accept 2 forms for master:
% Scalar: master = value
% else Vector: [master domain]
master = masterArg(1);
if sum(size(masterArg)) > 2
    domain = masterArg(2);
else
    domain = 0;
end

if domain
    masterStr = sprintf('%u.%u', master, domain);
else
    masterStr = sprintf('%u', master);
end

% Calculate slave alias and position, and format positionStr for
% display in the block graphics
if sum(size(indexArg)) > 2
    alias = indexArg(1);
    position = indexArg(2);
else
    alias = 0;
    position = indexArg(1);
end

if alias
    positionStr = sprintf('#%u:%u', alias, position);
else
    positionStr = num2str(position);
end

