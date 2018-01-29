require 'minitest/autorun'
require 'ldl'
require 'socket'
require 'timeout'

class TestGateway < Minitest::Test

    include LDL

    attr_reader :state
    attr_reader :s
    attr_reader :broker

    def rx_message
        msg, from = s.recvfrom(2048, 0)
        Semtech::Message.decode msg        
    end

    def setup
    
        Thread.abort_on_exception = true
        
        @s = UDPSocket.new
        s.bind('localhost', 0)
    
        LDL.const_set 'SystemTime', TimeSource.new
        
        SystemTime.start
        
        @broker = Broker.new    
        
        # - talk to localhost on the port we are listening to (above)
        # - shorten keepalive_interval so we can test fast
        # - shorten status_interval so we can test fast
        @state = Gateway.new broker, RandomEUI64.eui, host: 'localhost', port: s.local_address.ip_port, keepalive_interval: 1, status_interval: 1
        
        state.start
        
        sleep 0.1
        
    end
    
    def test_expect_keep_alive_on_start
        
        Timeout::timeout 0.5 do
        
            loop do
        
                msg = rx_message
        
                if msg.kind_of? Semtech::PullData
                    break
                end                
            
            end
            
        end
            
    end
    
    def test_expect_keep_alive_on_interval
        
        count = 0
        
        begin
        
            Timeout::timeout (state.keepalive_interval + 0.5) do
            
                loop do
            
                    msg = rx_message
            
                    if msg.kind_of? Semtech::PullData
                        
                        count += 1
                        
                    end                
                
                end
                
            end
            
        rescue
        end
        
        # should have two keep alives
        assert count == 2
            
    end
    
    def test_expect_status_on_start
        
        Timeout::timeout 0.5 do
        
            loop do
        
                msg = rx_message
        
                if msg.kind_of? Semtech::PushData and msg.stat
                    break
                end                
            
            end
            
        end
        
    end
    
    def test_expect_status_on_interval
        
        count = 0
        
        begin
        
            Timeout::timeout (state.keepalive_interval + 0.5) do
            
                loop do
            
                    msg = rx_message
            
                    if msg.kind_of? Semtech::PushData and msg.stat
                    
                        count += 1
                        
                    end                
                
                end
                
            end
            
        rescue
        end
        
        # should have two status
        assert count == 2
            
    end
    
    def test_upstream
    
        broker.publish({
                :data => "hello world",
                :freq => 0,
                :sf => 7,
                :bw => 125
            },
            "#{state.eui}"
        )
        
        Timeout::timeout 2 do
            
            loop do
        
                msg = rx_message
        
                if msg.kind_of? Semtech::PushData and not msg.rxpk.empty? and msg.rxpk.first.data == 'hello world'
                    break                
                end                
            
            end
            
        end
        
    end
    
    def test_downstream_immediate
    
    end
    
    def teardown    
        
        SystemTime.stop
        state.stop
        
        LDL.send(:remove_const, :SystemTime)
        
        @s.close
        
        Thread.abort_on_exception = false
        
    end
    
end
