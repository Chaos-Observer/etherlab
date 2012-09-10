function switch_etherlab(el_path)
% FUNCTION Switch to new EtherlabPath
%   Replaces all paths containing 'etherlab' with new path
%   The new path is not saved.
%   
%   switch_etherlab('/new/etherlab/path')

p = strjoin({el_path; 'rtw';'etherlab';'Contents.m'}, filesep);
try
    run(p);
catch
    error('%s is not a valid EtherLab path', el_path);
    return
end

        
% Split the path into a cell array
p = cellstr(char(java.lang.String(path).split(':')));

% Find all strings that contain etherlab
v = find(cell2mat(cellfun(@(x) ~isempty(x), ...
            strfind(p,'etherlab'),'uniformoutput',0)'));
        
for i = v
    rmpath(p{i});

    x = cellstr(char(java.lang.String(p{i}).split('/')));
    j = find(~cell2mat(cellfun(@(x) isempty(x), ...
            strfind(x,'rtw')', 'uniformoutput',0)));
        
    addpath([el_path,filesep,strjoin(x(j:end),filesep)]);
end

return

%%
function s = strjoin(x,delim)

% Delimiter cell array aufbauen
b = repmat({delim},size(x));
b{numel(b)} = ''; % Leztes element leeren

s = cell2mat(strcat(x,b)');
