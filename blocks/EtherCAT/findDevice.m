function EtherCATInfo = findDevice(EtherCATInfo,ProductCode,RevisionNo,debug)
    % Find an EtherCAT Device
    %
    % SYNOPSIS:
    %    EtherCATInfo = ...
    %       findDevice(EtherCATInfo,ProductCode,RevisionNo,debug)
    %
    % This function searches the EtherCATInfo structure to find the 
    % device specified by ProductCode and RevisionNo
    % * Omitting ProductCode or setting it to [] selects all devices
    % * Omitting RevisionNo or setting it to [] selects all devices 
    %   with the matching ProductCode
    % * Otherwise the device specified by ProductCode,RevisionNo is
    %   selected
    %
    % Setting debug shows some messages

    if nargin < 1
        help findDevice
        return
    end
    if nargin < 4
        debug = 0;
        if nargin < 3
            RevisionNo = [];
            if nargin < 2
                ProductCode = [];
            end
        end
    end
    
    if ~isnumeric(ProductCode) || ~isnumeric(RevisionNo)
        error('Arguments ProductCode and RevisionNo must be numeric')
    end
    args.ProductCode = ProductCode;
    args.RevisionNo = RevisionNo;

    for i = length(EtherCATInfo.Device):-1:1
        if debug
            fprintf('ProductCode #x%08x RevisionNo #x%08x... ', ...
                EtherCATInfo.Device(i).ProductCode, EtherCATInfo.Device(i).RevisionNo);
        end
        if ~ismatch(args, EtherCATInfo.Device(i).ProductCode, EtherCATInfo.Device(i).RevisionNo)
            EtherCATInfo.Device(i) = [];
            if debug
                fprintf('deleting\n')
            end
        elseif debug
            fprintf('selecting\n')
        end
    end
end

function x = ismatch(args,ProductCode,RevisionNo)
    %% Returns true when:
    %%    - args.ProductCode is missing
    %%       \ or ProductCode matches and (args.RevisionNo is missing
    %%                                      \ or RevisionNo matches)
    %% otherwise false
    %%
    %% The idea is that if a property is not set, it matches
    %% 
    if isempty(args.ProductCode) ...
            || isequal(args.ProductCode,ProductCode) ...
                && (isempty(args.RevisionNo) || isequal(args.RevisionNo,RevisionNo))
        x = true;
    else
        x = false;
    end
end
