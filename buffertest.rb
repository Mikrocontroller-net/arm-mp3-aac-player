require 'test/unit/assertions'

include Test::Unit::Assertions

def get_writable_buffer()
  if @buffers[@wp] == :busy
    # check if buffer is really busy
    # dma_address = wp || next_dma_address = wp
    @buffers[@wp] = :empty
  end

  if @buffers[@wp] == :empty
    buffer = @wp
    @wp = (@wp + 1) % MAX_BUFFERS
    return buffer
  else
    return false
  end
end

def set_buffer_ready(i)
  @buffers[i] = :ready
end

def set_buffer_busy(i)
  @buffers[i] = :busy
end

def get_readable_buffer()
  if @buffers[@rp] == :ready
    buffer = @rp
    @rp = (@rp + 1) % MAX_BUFFERS
    return buffer
  else
    return false
  end
end

def show()
  puts @buffers.inspect
  puts "wp: #{@wp}"
  puts "rp: #{@rp}"
end

MAX_BUFFERS = 4
@buffers = [:empty] * MAX_BUFFERS
@wp = 0
@rp = 0

buffer = get_writable_buffer()
# work with buffer
set_buffer_ready(buffer)
show

buffer = get_writable_buffer()
# work with buffer
set_buffer_ready(buffer)
show

buffer = get_readable_buffer()
# start DMA transfer
set_buffer_busy(buffer)
show

buffer = get_readable_buffer()
# start DMA transfer
set_buffer_busy(buffer)
show

buffer = get_readable_buffer()
show
assert_equal(false, buffer)

buffer = get_writable_buffer()
# work with buffer
set_buffer_ready(buffer)
show

buffer = get_writable_buffer()
# work with buffer
set_buffer_ready(buffer)
show

buffer = get_writable_buffer()
# work with buffer
set_buffer_ready(buffer)
show

buffer = get_writable_buffer()
# work with buffer
set_buffer_ready(buffer)
show

buffer = get_writable_buffer()
# work with buffer
show
assert_equal(false, buffer)
