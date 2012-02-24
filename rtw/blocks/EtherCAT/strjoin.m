function s = strjoin(x,delim)
if size(x,1) == 1
    x = x';
end

% Delimiter cell array aufbauen
b = repmat({delim},size(x));
b{numel(b)} = ''; % Leztes element leeren

s = cell2mat(strcat(x,b)');
end