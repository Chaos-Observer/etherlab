function rv = mts_temposonics(model, offset, filter, tau, sensorlen,...
                             status, do_revert, velocity_output)

entries = [...
           hex2dec('3101'),  1, 16, 1016; ...  
           hex2dec('3101'),  2, 32, 1032; ...  
           hex2dec('3101'),  3, 32, 2032; ...  
        
           hex2dec('3101'),  5, 16, 1016; ...  
           hex2dec('3101'),  6, 32, 1032; ...  
           hex2dec('3101'),  7, 32, 2032; ...  
          
           hex2dec('3101'),  9, 16, 1016; ...  
           hex2dec('3101'), 10, 32, 1032; ...  
           hex2dec('3101'), 11, 32, 2032; ...  
          
           hex2dec('3101'), 13, 16, 1016; ...  
           hex2dec('3101'), 14, 32, 1032; ...  
           hex2dec('3101'), 15, 32, 2032; ...  
           
           hex2dec('3101'), 17, 16, 1016; ...  
           hex2dec('3101'), 18, 32, 1032; ...  
           hex2dec('3101'), 19, 32, 2032; ... 
];

        
 %                   TxPdoSt TxPdoEnd StatusStart StatusEnd                          
pdo = [...               
        hex2dec('1a00'),  1,   15 ;...
];
                                                                            
%   Model       ProductCode          Revision                 RxSt|RxEnd|TxSt|TxEn|func


rv.SlaveConfig.vendor = 2;       

rv.SlaveConfig.product = hex2dec('26483052');

num_all_pdos = 3*model; 

% set scale for double outputs
scale_int = 2^15;

% RxPdo SyncManager
rv.SlaveConfig.pdo = { {3 1 {}} };
rv.SlaveConfig.pdo{1}{3} = {pdo(1,1), entries(1:3*model,:)};


 % Fill in Offsets
if (isempty(offset) || numel(offset)==1 || numel(offset) == model)   
    rv.PortConfig.output(1).offset = {'OffsetPosition', offset};
     % if input is wrong, fill with emptys
else 
      warning('EtherLAB:Beckhoff:EL31xx:offset', ['The dimension of the'...
    ' offset output does not match to the number of elements of the'...
    ' terminal. Please choose a valid output, or the offset is being ignored'])
 
end

 % Fill in Filters 
if (isempty(tau) || numel(tau)==1 || numel(tau) == model)   
    if isempty(find(tau <= 0))
        rv.PortConfig.output(1).filter = {'Filter', tau};
    else
        errordlg(['Specify a nonzero time constant '...
                  'for the output filter'],'Filter Error');
    end
else 
      warning('EtherLAB:Beckhoff:EL31xx:tau', ['The dimension of the'...
    ' filter output does not match to the number of elements of the'...
    ' terminal. Please choose a valid output, or the filter is being ignored'])
 
end


% Populate the block's output port(s)

r = 0 : model-1;
rv.PortConfig.output.pdo = [zeros(numel(r),4)];
rv.PortConfig.output.pdo(:,2) = [3*r];

rv.PortConfig.output(1).full_scale = 1000000;


if velocity_output
    status_pdo = 3;
else
    status_pdo = 2;
end

% Port Config velocity and state output output 

if velocity_output
    rv.PortConfig.output(2).gain = 1000;
    rv.PortConfig.output(2).pdo = [zeros(numel(r),4)];
    rv.PortConfig.output(2).pdo(:,2) = [3*r+1];
    rv.PortConfig.output(2).fullscale = 2^30;
end

if status
    rv.PortConfig.output(status_pdo).pdo = [zeros(numel(r),4)];
    rv.PortConfig.output(status_pdo).pdo(:,2) = [3*r+2];
end




