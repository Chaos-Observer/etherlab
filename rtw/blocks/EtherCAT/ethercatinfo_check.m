function changed = ethercatinfo_check(block)

if nargin < 1
    block = gcb;
end

changed = false;

ethercatinfo = get_param(block, 'ethercatinfo');

UserData = get_param(block,'UserData');
if ~isempty(UserData) && ~isstruct(UserData)
    % Report error that UserData is not empty and not a structure
    errordlg(['UserData for ' block ' is not of type struct']);
    return
end

if isempty(ethercatinfo)
    if isfield(UserData,'ei') && ~isempty(UserData.ei) || ...
            isfield(UserData,'file') && ~isempty(UserData.file)
        changed = true;
        UserData.ei = [];
        UserData.file = [];
        set_param(block,'UserData', UserData);
    end
    return
end

try
    ei = evalin('base',ethercatinfo);
catch
    % Report error that ethercatinfo cannot be evaluated
    disp(['could not evaluate ' ethercatinfo]);
    return
end

if isa(ei, 'EtherCATInfo')
    UserData.ei = ei;
    UserData.file = [];
    changed = true;
    set_param(block,'UserData', UserData);
elseif isempty(ei)
    if isfield(UserData,'ei') && ~isempty(UserData.ei) || ...
            isfield(UserData,'file') && ~isempty(UserData.file)
        changed = true;
        UserData.ei = [];
        UserData.file = [];
        set_param(block,'UserData', UserData);
    end
elseif ischar(ei)
    name = ei;
    
    d = dir(name);
    if isempty(d)
        errordlg(['EtherCat Info file "' name '" does not exist for block ' block]);
        return
    end
    
    if isempty(UserData) || ~isfield(UserData,'ei') || ...
            ~isfield(UserData,'dir') || ~isequal(UserData.dir, d)

        try
            UserData.ei = EtherCATInfo(name);
        catch
            errordlg(['EtherCat Info file "' name '" for block ' block ' is invalid']);
            return
        end
        UserData.dir = d;

        changed = true;
        set_param(block,'UserData',UserData);
    end
else
    % Report error
    errordlg(['EtherCAT Info file for block "' block ...
        '" must either be a character string or evaluate to an EtherCATInfo object.']);
end
