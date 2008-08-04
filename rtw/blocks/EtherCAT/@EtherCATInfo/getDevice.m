function ei = getDevice(varargin)
% GETDEVICE: finds devices in the object
% Synopsis: device = getDevice(EtherCATInfo, Product, Revision)
% Arguments:
%   EtherCATInfo: the object
%   Product: Numeric array - selects all Devices with 
%               matching ProductCode
%            string cell array - selectes Devices with matching Types
%   Revision: Numeric array - Device's RevisionNo must match
%             'first'   - first device
%             'last'    - last device
%             'latest'  - selects all devices not hidden by others
ei = varargin{1};

device = repmat(XML_ParseSlave,1,0);

%% Setup the selection options

% RevisionNo is either a string containing 'first' 'last' 'latest'
% or a list of numbers of revision numbers to check for.
% This option is specified by the third option
RevisionNo = [];
if nargin >= 3
    % If revision is a string, make sure it is valid, otherwise
    % it has to be numeric
    if isa(varargin{3},'char') ...
            && isempty(cell2mat( ...
                 strfind({'first' 'last' 'latest'}, varargin{3}))) ...
            || ~isa(varargin{3},'char') && ~isnumeric(varargin{3})
        error('RevisionNo incorrect');
    end
    RevisionNo = varargin{3};
end

% ProductCode and ProductString are exclusive lists that contain
% ProductCodes or Type strings respectively. If specified,
% only devices that match either are included. 
% These variables are derived from parameter 2
ProductCode = [];
ProductString = {};
if nargin >= 2
    if isa(varargin{2},'cell') && length(varargin{2}) ...
            && isa(varargin{2}{1},'char')
        % Param 2 is a cell array
        ProductString = varargin{2};
    elseif isa(varargin{2},'char')
        % Param 2 is a string, convert it to cell
        ProductString = {varargin{2}};
    elseif isnumeric(varargin{2})
        % Param 2 is a list of ProductCodes
        ProductCode = varargin{2};
    else
        error(['Specify product as a numeric array of ' ...
            'product codes or a cell array of strings']);
    end
end

% If ProductCode and ProductString is empty, it doesn't make sense
% to check for RevisionNo, so empty it
if isempty(ProductCode) && isempty(ProductString) && ...
        (isnumeric(RevisionNo) || strcmp(RevisionNo, 'latest'))
    RevisionNo = [];
end

%% Find the devices according to the specification

idx = 1;
exclude = [];
for i = 1:length(ei.Descriptions.Devices.Device)
    dev = ei.Descriptions.Devices.Device(i);
    
    rev = dev.Type.RevisionNo;
    type = dev.Type.TextContent;
    pc = dev.Type.ProductCode;
    
    % If ProductString is specified, add it if it appears on the list
    %    or if ProductCode is specified, add it if on the list
    %    or add device if both ProductString and ProductCode are empty
%     if ~isempty(ProductString) && ~isempty(cell2mat(strfind(ProductString, type)))...
%             || ~isempty(ProductCode) && ~isempty(find(ProductCode == pc, 1)) ...
%             || isempty(ProductString) && isempty(ProductCode)
    if ProductCodeMatch(ProductCode, pc) && ProductStringMatch(ProductString,type)
        
        % Update exclude list
        exclude = [exclude dev.HideType];
        
%         if isa(RevisionNo, 'char') ...
%                 || isempty(RevisionNo) ...
%                 || ~isempty(find(RevisionNo == rev,1))
        if RevisionNoMatch(RevisionNo, rev)
            device(idx) = dev;
            if ~strcmp(RevisionNo, 'last')
                % Don't increase index if last should be returned
                % Causes the first element to be overwritten
                idx = idx + 1;
            end
            if strcmp(RevisionNo, 'first')
                break
            end
        end
    end
end

% If 'latest' Revision is requested, remove items that exist in 
% exclude list
if strcmp(RevisionNo, 'latest')
    for i = length(device):-1:1
        if find(exclude == device(i).RevisionNo,1)
            device(i) = [];
        end
    end
end

ei.Descriptions.Devices.Device = device;

%% Matching functions

function i = ProductStringMatch(ProductString, candidate)
i = false;
if isempty(ProductString) || ~isempty(find(strcmp(ProductString, candidate),1))
    i = true;
end

function i = ProductCodeMatch(ProductCode, candidate)
i = false;
if isempty(ProductCode) || ~isempty(find(ProductCode == candidate,1))
    i = true;
end

function i = RevisionNoMatch(RevisionNo, candidate)
i = false;
if isa(RevisionNo, 'char') ...
        || isempty(RevisionNo) || ~isempty(find(RevisionNo == candidate,1))
    i = true;
end
