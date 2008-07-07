function device = EtherCATDevice(varargin)
% EtherCATDevice: Constructor function for EtherCATDevice object
% EtherCATDevice = EtherCATDevice(xmlnode)
switch nargin
    case 0
        % if no input arguments, create a default object
        device = ParseXML;
    case 1
        % if single argument of class asset, return it
        if isa(varargin{1},'EtherCATDevice')
            device = varargin{1};
            return
        elseif isa(varargin{1},...
                'org.apache.xerces.dom.DeferredElementImpl')
            device = ParseXML(varargin{1});
        else
            error('Wrong argument type')
        end
    otherwise
        error('Wrong number of input arguments')
end

device = class(device,'EtherCATDevice');
