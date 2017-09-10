require 'minitest/autorun'
require 'ldl'

class TestPushAck < Minitest::Test

    include LDL

    def setup
        @state = Semtech::PushAck.new
    end

    def test_encode_default    
        
        out = @state.encode    
        
        iter = out.unpack("CS>C").each
        
        assert_equal Semtech::Message::VERSION, iter.next
        iter.next
        
        assert_equal @state.class.type, iter.next
        
    end
    
    def test_decode_default
        
        input = @state.encode
        
        input = "\x02\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00{\"rxpk\":[]}"
        
        decoded = Semtech::PushAck.decode(input)
        
        assert_equal 0,  decoded.token
        
    end

end
