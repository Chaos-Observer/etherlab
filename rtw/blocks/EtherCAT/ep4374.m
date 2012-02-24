function output = ep4374(command,varargin)

    switch command
        case 'process'
            output = prepare_config(varargin{:});
        case 'dtype'
            en = cell2struct(...
                   get_param(gcb,'MaskEnables'),...
                   get_param(gcb,'MaskNames')...
                 );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );            
            % Gain and Offset are only allowed with this Data Type
            if strcmp(values.dtype, 'Double with scale and offset');
                en.input_scale = 'on';
                en.input_offset = 'on';
            else
                en.input_scale = 'off';
                en.input_offset = 'off';
                if ~strcmp(get_param(gcb, 'input_scale'), '1')
                    set_param(gcb, 'input_scale', '1');
                end
                if ~strcmp(get_param(gcb, 'input_offset'), '0')
                    set_param(gcb, 'input_offset', '0');
                end
            end
            if strcmp(values.dtype, 'Raw bits')
                set_param(gcb,'filter','off');
                en.filter = 'off';
                en.tau = 'off';
            else
                en.filter = 'on';
            end
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));  
            output = [];
        case 'filter'
            en = cell2struct(...
                   get_param(gcb,'MaskEnables'),...
                   get_param(gcb,'MaskNames')...
                 );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );                        
            if (strcmp(values.filter,'on'))
                en.tau = 'on';
            else
                en.tau = 'off';
            end
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));   
            output = [];
        case 'dc'
            en = cell2struct(...
                get_param(gcb,'MaskEnables'),...
                get_param(gcb,'MaskNames')...
                );
            values = cell2struct(...
                get_param(gcb,'MaskValues'),...
                get_param(gcb,'MaskNames')...
                );
            if strcmp(values.dcmode, 'DC-Customized');
                en.dccustomer = 'on';
            else
                en.dccustomer = 'off';
            end;
            set_param(gcb,'MaskEnables',struct2cell(en));
            set_param(gcb,'MaskValues',struct2cell(values));                                              
            output=[];
        otherwise
            disp('Unknown method.')
            output = [];
    end
end



function rv = prepare_config(model,io_type, dtype, output_scale, output_offset, ...
                     filter, tau, type_1,  type_2, type_3, type_4, input_scale,... 
                     dcmode, dccustomer)

entries = [...
           hex2dec('7020'), 17, 16, 2016; ...  %RxPdo's
           hex2dec('7030'), 17, 16, 2016; ...

           hex2dec('6000'), 17, 16, 2016; ...  %TxPdo's
           hex2dec('6010'), 17, 16, 2016; ...
          ];

        
 %                   TxPdoSt TxPdoEnd StatusStart StatusEnd                          
pdo = [...               
        hex2dec('1600'),  1,    1 ;...
        hex2dec('1601'),  2,    2 ;...
        
        hex2dec('1a01'),  3,    3 ;...
        hex2dec('1a03'),  4,    4 ;...
];
                                                                            
%   Model       ProductCode          Revision                 RxSt|RxEnd|TxSt|TxEn|func

 models = struct(...
  'EP43740002r00100002' ,[hex2dec('11164052'), hex2dec('00100002'),  1,  2,  3,  4, 1] ...
    );          
                        

rv.SlaveConfig.vendor = 2;       

product = models.(model);            
rv.SlaveConfig.product = product(1); 
rv.SlaveConfig.description = model; 

% Determine Function Output
func = {'differential', 'single end'};
rv.output_func = func{product(7)};


dir.output = struct(...
        'Sm',3, ...
        'Dir',1, ...
        'pdoindex', [product(5):product(6)], ...
        'number_pdo', product(6)-product(5)+1, ... 
        'scale', output_scale ...
);
        
dir.input = struct(...
        'Sm',2, ...
        'Dir',0, ...
        'pdoindex', [product(3):product(4)], ...
        'number_pdo', product(4)-product(3)+1, ...
        'scale', input_scale ... 
);

num_all_pdos = dir.input.number_pdo + dir.output.number_pdo; 

% Set the output type for each port
basetype = [type_1-1; type_2-1; type_3-1; type_4-1];  
for i=1:numel(basetype)
    if basetype(i)==3 
        basetype(i) = 6; %0-10V
    end
end

% Configure Sdo's for output types

rv.SlaveConfig.sdo = cell(num_all_pdos,4);
for k = 1:num_all_pdos
    rv.SlaveConfig.sdo{k,1} = hex2dec('F800');
    rv.SlaveConfig.sdo{k,2} = k;
    rv.SlaveConfig.sdo{k,3} = 16;
    rv.SlaveConfig.sdo{k,4} = basetype(k);
end    

% set DC mode 
% 
%[AssignActivate, CycleTimeSync0, CycleTimeSync0Factor, ShiftTimeSync0,...
% ShiftTimeSync0Factor, ShiftTimeSync0Input, CycleTimeSync1, CycleTimeSync1Factor,...
% ShiftTimeSync1, ShiftTimeSync1Factor]   

dcstate = [              0, 0, 0, 0, 0, 0, 0, 1,     0, 0;...
            hex2dec('700'), 0, 1, 0, 0, 0, 0, 1, 80000, 0;...
];


if dcmode == 3 % DC Customer
    rv.SlaveConfig.dc = dccustomer;
else 
     %Set Index of DC Mode or set [Index AssignActivate cycletimesync0 factor0 cycletimeshift0 input0 cycletimesync1 factor1 cylcetimeshit1]
    rv.SlaveConfig.dc = dcstate(dcmode,:); 
end

% set scale for double outputs
scale_int = 2^15;


% Populate the RxPDO Inputs structure

io = {'output','input'};

for pdos = 1:2

% RxPdo SyncManager
rv.SlaveConfig.sm(pdos) = { {dir.(io{pdos}).Sm dir.(io{pdos}).Dir {}} };

rv.SlaveConfig.sm{pdos}{3} = arrayfun(...
        @(x) {pdo(x,1), entries(pdo(x,2):pdo(x,3),:)}, ...
        dir.(io{pdos}).pdoindex, ...
        'UniformOutput',0 ...
);


if ( strcmp(dtype, 'Double with scale and offset')&&strcmp(io{pdos},'output') || ...
                  strcmp(io{pdos},'input'))           
% Fill in Scale
    scale = dir.(io{pdos}).scale;
    if (isempty(scale) || numel(scale)==1 || numel(scale) == dir.(io{pdos}).number_pdo)   
        if strcmp(io_type,'Separate Outputs')        
            for k = 1:dir.(io{pdos}).number_pdo
                if numel(scale) == 1
                    rv.PortConfig.(io{pdos})(k).gain = {'Gain', scale};
                elseif numel(scale) == dir.(io{pdos}).number_pdo 
                    rv.PortConfig.(io{pdos})(k).gain = {'Gain', scale(k)};
                else
                    rv.PortConfig.(io{pdos})(k).gain = {'Gain', []};
                end
             end
        else
            rv.PortConfig.(io{pdos}).gain = {'Gain', scale};
        end
         % if input is wrong, fill with emptys
    else 
        warning('EtherLAB:Beckhoff:EL31xx:scale', ['The dimension of the'...
        ' scale output does not match to the number of elements of the'...
        ' terminal. Please choose a valid output, or the scale is being ignored'])
    end
end

    % Set data type scale
if ~strcmp(dtype, 'Raw Bits')||strcmp(io{pdos},'input')
    if strcmp(io_type, 'Separate Outputs')
        for k = 1:dir.(io{pdos}).number_pdo
            rv.PortConfig.(io{pdos})(k).full_scale = scale_int; 
        end
    else
            rv.PortConfig.(io{pdos}).full_scale = scale_int; 
    end
end
%end

if strcmp(io{pdos},'output')   
 
    % Fill in Offsets
    if filter
        if (isempty(tau) || numel(tau)==1 || numel(tau) == dir.(io{pdos}).number_pdo)
            if isempty(find(tau <= 0))   
                if strcmp(io_type,'Separate Outputs')        
                    for k = 1 : dir.(io{pdos}).number_pdo
                        if numel(tau) == 1
                            rv.PortConfig.output(k).filter = {'Filter', tau};
                        elseif numel(tau) == dir.(io{pdos}).number_pdo 
                            rv.PortConfig.output(k).filter = {'Filter', tau(k)};
                        else
                            rv.PortConfig.output(k).filter = [];
                        end
                    end 
                else
                    rv.PortConfig.output.filter = {'Filter', tau};
                end
            else
                errordlg(['Specify a nonzero time constant '...
                         'for the output filter'],'Filter Error');
            end
         % if input is wrong, fill with emptys
        else 
            if strcmp(io_type,'Separate Outputs')
                for k = 1 : dir.(io{pdos}).number_pdo
                    rv.PortConfig.output(k).filter = [];
                end
            else
               rv.PortConfig.output.filter = {'Filter', tau};
            end
               warning('EtherLAB:Beckhoff:EL31xx:filter', ['The dimension of the'...
               ' filter output does not match to the number of elements of the'...
               ' terminal. Please choose a valid output, or the filter is being ignored'])
        end
    end
     
     
    % set offset 
    if strcmp(dtype, 'Double with scale and offset')
        % Fill in Offsets
        if (isempty(output_offset) || numel(output_offset)==1 || numel(output_offset) == dir.(io{pdos}).number_pdo)   
            if strcmp(io_type,'Separate Outputs')        
                for k = 1 : dir.(io{pdos}).number_pdo
                    if numel(output_offset) == 1
                        rv.PortConfig.output(k).offset = {'Offset', output_offset};
                    elseif numel(output_offset) == dir.(io{pdos}).number_pdo 
                        rv.PortConfig.output(k).offset = {'Offset', output_offset(k)};
                    else
                        rv.PortConfig.output(pdos).offset = [];
                    end
                 end
            else
                rv.PortConfig.output.offset = {'Offset', output_offset};
            end
             % if input is wrong, fill with emptys
        else 
            if strcmp(io_type,'Separate Outputs')
                for k = 1 : dir.(io{pdos}).number_pdo
                    rv.PortConfig.output(k).offset = [];
                end
            else
                rv.PortConfig.output.offset = {'Offset', output_offset};
            end
            warning('EtherLAB:Beckhoff:EL31xx:output_offset', ['The dimension of the'...
            ' offset output does not match to the number of elements of the'...
            ' terminal. Please choose a valid output, or the offset is being ignored'])
        end
    end
end


% Populate the block's output port(s)

r = 0:dir.(io{pdos}).number_pdo-1;
if strcmp(io_type, 'Vector Output')
        rv.PortConfig.(io{pdos}).pdo = [zeros(numel(r),4)];
        rv.PortConfig.(io{pdos}).pdo(:,2) = [r];
        rv.PortConfig.(io{pdos}).pdo(:,1) = [r(pdos)];
else
    for k = 1 : dir.(io{pdos}).number_pdo
        rv.PortConfig.(io{pdos})(k).pdo = [r(pdos), r(k), 0, 0];
    end
end

end

end



