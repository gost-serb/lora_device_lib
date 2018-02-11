module LDL

    class Radio
    
        attr_accessor :buffer, :mode, :broker, :mac
    
        def initialize(mac, broker)

            raise "SystemTime must be defined" unless defined? SystemTime
            
            @buffer = ""     
            @broker = broker 
            @mac = mac
            
        end
    
        def resetHardware
            SystemTime.wait(1)        
        end
    
        def transmit(data, **settings)            
            
            bw = settings[:bw]
            sf = settings[:sf]
            freq = settings[:freq]
            
            msg = {
                :eui => mac.devEUI, 
                :time => SystemTime.time,
                :data => data.dup,
                :sf => sf,
                :bw => bw,
                :cr => settings[:cr],
                :freq => freq,
                :power => settings[:power],                    
                :channel => settings[:channel]
            }
            
            broker.publish msg, "tx_begin"
            
            SystemTime.onTimeout(mac.class.transmitTimeUp(bw, sf, data.size)) do
            
                mac.io_event :tx_complete, SystemTime.time
                                
                broker.publish({:eui => mac.devEUI}, "tx_end")
                
            end           
            
            true
        
        end
            
        def receive(**settings)
            
            bw = settings[:bw]
            sf = settings[:sf]
            freq = settings[:freq]
            
            tx_begin = nil
            
            t_sym = (2 ** settings[:sf]) / settings[:bw].to_f
            
            to = SystemTime.onTimeout( settings[:timeout] * t_sym ) do
               
               broker.unsubscribe tx_begin
               
               mac.io_event :rx_timeout, SystemTime.time 
               
            end
            
            puts "radio listening at #{SystemTime.time} ticks"
            puts
            
            rx_begin = broker.subscribe "tx_begin" do |m1|
            
                if m1[:sf] == sf and m1[:bw] == bw and m1[:freq] == freq
            
                    puts "got something"
            
                    broker.unsubscribe to
                    broker.unsubscribe rx_begin
                    
                    rx_end = broker.subscribe "tx_end" do |m2|            
                    
                        if m1[:eui] == m2[:eui]
                        
                            broker.unsubscribe rx_end
                            
                            mac.io_event :rx_ready, SystemTime.time
                            
                            @buffer = m1[:data].dup
                        
                        end
                        
                    end
                    
                end
            
            end
            
            true
                    
        end
        
        def collect        
            buffer
        end
        
        def sleep
        end
        
    end


end
