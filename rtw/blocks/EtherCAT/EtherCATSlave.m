classdef EtherCATSlave

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    methods (Static)
        %====================================================================
        function port = configurePorts(name,pdo,type,vector,scale,bits)

            if nargin < 5
                scale = [];
            end
            if nargin < 6
                bits = 0;
            end

            pdo_count = size(pdo,1);

            if vector
                port.pdo = pdo;
                port.pdo_data_type = type;
                port.portname = name;

                if isa(scale,'struct')
                    if bits
                        port.full_scale = 2^bits;
                        port.gain   = {'Gain',  scale.gain};
                        port.offset = {'Offset',scale.offset};
                        port.filter = {'Filter',scale.tau};
                    else
                        port.full_scale = [];
                        port.gain   = [];
                        port.offset = [];
                        port.filter = [];
                    end
                end
            else
                port = arrayfun(...
                        @(i) struct('pdo',pdo(i,:),...
                                    'pdo_data_type',type,...
                                    'portname', [name,int2str(i)]), ...
                        1:pdo_count);

                if isa(scale,'struct')
                    port(1).full_scale = [];
                    port(1).gain   = [];
                    port(1).offset = [];
                    port(1).filter = [];
                end

                if ~bits
                    return
                end

                % Replicate gain, offset and tau if there is only on element
                if numel(scale.gain) == 1
                    scale.gain = repmat(scale.gain,1,pdo_count);
                end
                if numel(scale.offset) == 1
                    scale.offset = repmat(scale.offset,1,pdo_count);
                end
                if numel(scale.tau) == 1
                    scale.tau = repmat(scale.tau,1,pdo_count);
                end

                for i = 1:pdo_count
                    idxstr = int2str(i);

                    port(i).full_scale = 2^bits;

                    if i <= numel(scale.gain)
                        port(i).gain   = {['Gain', idxstr], scale.gain(i)};
                    end

                    if i <= numel(scale.offset)
                        port(i).offset = {['Offset', idxstr], scale.offset(i)};
                    end

                    if i <= numel(scale.tau)
                        port(i).filter = {['Filter', idxstr], scale.tau(i)};
                    end
                end
            end
        end
    end

    %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    properties (Constant, Abstract)
        models
        dc
    end
end
