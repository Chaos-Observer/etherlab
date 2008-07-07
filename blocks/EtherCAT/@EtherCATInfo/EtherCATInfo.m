function ei = EtherCATInfo(varargin)
% EtherCATInfo: Constructor function for EtherCATInfo object
% EtherCATInfo = EtherCATInfo(xmlfile)
    switch nargin
        case 0
            % if no input arguments, create a default object
            ei = ParseXML;
        case 1
            % if single argument of class asset, return it
            if isa(varargin{1},'EtherCATInfo')
                ei = varargin{1};
            elseif isa(varargin{1},'char')
                ei = ParseXML(varargin{1});
            else
                error('Wrong argument type')
            end
        otherwise
            error('Wrong number of input arguments')
    end
    ei = class(ei,'EtherCATInfo');
end

