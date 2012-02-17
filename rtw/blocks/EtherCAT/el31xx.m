function rv = el31xx(model,output_type, status, dtype, scale, offset, filter, tau)

%model = 'EL2032v2'
%diagnose = 1;
%input_type = 'Vector Input'

entries = [...
        hex2dec('3101'), 2, 16, 2016; ... 
        hex2dec('3101'), 1,  8, 1008; ...  
        hex2dec('3102'), 2, 16, 2016; ...
        hex2dec('3102'), 1,  8, 1008; ...
        hex2dec('6401'), 1, 16, 2016; ... %Channel outputs without states
        hex2dec('6401'), 2, 16, 2016; ...
          ];

        

pdo = [...
        hex2dec('1a00'),  1,  2;...       %TxPdo Sm3 El3102, El3142, EL3152, EL3162
        hex2dec('1a01'),  3,  4;...       %TxPdo Sm3 El3102, El3142, EL3152, EL3162
        hex2dec('1a10'),  5,  6;...       %TxPdo Sm3 El3102, El3142, EL3152, EL3162 
      ];

%   Model       ProductCode          Revision       TxStart|TxEnd|TxStart | TxEnd
%                                                     with   with without  without
%                                                              State
models = struct(...
  'EL3102'  ,[hex2dec('0C1E3052'), hex2dec('00000000'), 1,    2,     3,      3 ],...
  'EL3142'  ,[hex2dec('0C463052'), hex2dec('00000000'), 1,    2,     3,      3 ],...
  'EL3152'  ,[hex2dec('0C503052'), hex2dec('00000000'), 1,    2,     3,      3 ],...
  'EL3162'  ,[hex2dec('0C5A3052'), hex2dec('00000000'), 1,    2,     3,      3 ]...
    );

rv.SlaveConfig.vendor = 2;

product = models.(model);
rv.SlaveConfig.product = product(1);

% RxPdo SyncManager
rv.SlaveConfig.pdo = { {3 1 {}} };

% Choose required pdos 
if  status 
    pdoindex = product(3):product(4)
else
    pdoindex = product(5):product(6)
end
   
% Populate the RxPDO Inputs structure
rv.SlaveConfig.pdo{1}{3} = arrayfun(...
        @(x) {pdo(x,1), entries(pdo(x,2):pdo(x,3),:)}, ...
        pdoindex, ...
        'UniformOutput',0 ...
);

% set scale for double outputs
scale_int = 2^15;
number_elements = 2;

% Set scale and offset for each data type
rv.PortConfig.output = struct(...
    'gain', [],... 
    'offset', [],...
    'filter', [],...
    'full_scale', []...
    );



% set scale and offset 
if strcmp(dtype, 'Double with scale and offset')
    % Check vor valiability of variable outputs
    % Set Scales
    if  strcmp(output_type,'Seperate Outputs') &&...
            (isempty(scale) || numel(scale)==1 || numel(scale) == number_elements)
            if numel(scale) == 1
                rv.PortConfig.output.scale = ...
                    {'Scale', [ones(1,number_elements)]*scale};
            else
                rv.PortConfig.output.scale = {'Scale', scale};
            end
     elseif strcmp(output_type,'Vector Output') &&...
             (isempty(scale) || numel(scale)==1)
              rv.PortConfig.output.scale = {'Scale', scale};
     else
        warning('EtherLAB:Beckhoff:EL31xx:scale', ['The dimension of the'...
       ' scale output does not match to the number of elements of the'...
       ' terminal. Please choose a valid output, or the scale is being ignored'])
    end
    % Set Offsets
    if  strcmp(output_type,'Seperate Outputs') &&...
            (isempty(offset) || numel(offset)==1 || numel(offset) == number_elements)
            if numel(offset) == 1
                rv.PortConfig.output.offset = ...
                    {'Offset', [ones(1,number_elements)]*offset};
            else
                rv.PortConfig.output.offset = {'Offset', offset};
            end
     elseif strcmp(output_type,'Vector Output') &&...
             (isempty(offset) || numel(offset)==1)
              rv.PortConfig.output.offset = {'Offset', offset};
     else
        warning('EtherLAB:Beckhoff:EL31xx:offset', ['The dimension of the'...
       ' offset output does not match to the number of elements of the'...
       ' terminal. Please choose a valid output, or the offset is being ignored'])
     end 
end


% Set data type scale
if ~strcmp(dtype, 'Raw Bits')
    rv.PortConfig.output.full_scale = scale_int; 
end

% define filter if choosen
if filter && ~strcmp(dtype, 'Raw bits') 
    if numel(filter) == 1
        if filter < 0
            errodlg(['Specify a nonzero time constant '...
                        'for the output filter'],'Filter Error');
        else
            rv.PortConfig.output.filter = {'Filter', tau};
        end
   else
       warning('EtherLAB:Beckhoff:EL31xx:filter', ['Filter output must be a'...
               ' scalar. Please choose a valid output, or the filter is being'...
               ' ignored']);    
   end
end


% Populate the block's output port(s)
r = 0:(product(4) - product(3));
if strcmp(output_type, 'Vector Output')
    rv.PortConfig.output.pdo = [zeros(numel(r),4)];
    rv.PortConfig.output.pdo(:,1) = [0];
    rv.PortConfig.output.pdo(:,2) = [r];
else
    rv.PortConfig.output = arrayfun(@(x) struct('pdo', [0, x, 0, 0]), r);
end


if status && strcmp(output_type, 'Vector Output')
   rv.PortConfig.output(2).offset = {'Offset', []}
   rv.PortConfig.output(2).scale = {'Scale', []}
   rv.PortConfig.output(2).pdo = [zeros(1,4)];
end
