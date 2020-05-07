module spi_flash_mem_ctrl #(
    parameter int ADDR_BITS
) (
    input sck,
    input cs,
    input reset,
    input [3:0] dq_in,
    output [3:0] dq_drive,
    output [3:0] dq_out,
    output mem_req_valid,
    output [ADDR_BITS-1:0] mem_req_addr,
    output [7:0] mem_req_data,
    output mem_req_r_wb,
    input [7:0] mem_resp_data
);

    // CAUTION! This model only supports a small subset of standard QSPI flash
    // features. It is useful for modeling a pre-loaded flash memory intended
    // to be used as a ROM. You should replace this with a verilog model of
    // your specific flash device for more rigorous verification. It also very
    // likely contains bugs. Use at your own risk!

    // Supported SPI instructions
    // 3-byte address reads
    localparam CMD_READ               = 8'h03; // No dummy
    localparam CMD_FAST_READ          = 8'h0B;
    localparam CMD_QUAD_O_FAST_READ   = 8'h6B;
    localparam CMD_QUAD_IO_FAST_READ  = 8'hEB;
    // 4-byte address reads
    localparam CMD_READ4              = 8'h13; // No dummy
    localparam CMD_FAST_READ4         = 8'h0C;
    localparam CMD_QUAD_O_FAST_READ4  = 8'h6C;
    localparam CMD_QUAD_IO_FAST_READ4 = 8'hEC;
    // No writes/erases are supported by this model yet
    // No register reads are supported by this model yet

    // SPI mode settings for clock polarity and phase
    localparam CPOL = 1'b0;
    localparam CPHA = 1'b0;

    // SPI flash behavior settings
    // Often the number of dummy cycles depends on the instruction, but here
    // we'll make it all uniform.
    localparam DUMMY_CYCLES = 8;

    // State
    reg [2:0] state;
    logic [2:0] state_next;
    // States
    localparam STANDBY   = 3'd0;
    localparam GET_CMD   = 3'd1;
    localparam GET_ADDRS = 3'd2;
    localparam GET_ADDRQ = 3'd3;
    localparam DUMMY     = 3'd4;
    localparam PUT_DATAS = 3'd5;
    localparam PUT_DATAQ = 3'd6;
    localparam ERROR     = 3'd7;

    // Incoming data
    reg [31:0] data_buf;
    logic [31:0] data_buf_next;
    // Incoming data bit count
    reg [5:0] data_count;
    logic [5:0] data_count_next;
    // Dummy cycle counter
    reg [7:0] dummy;
    logic [7:0] dummy_next;
    // Command
    reg [7:0] cmd;
    logic [7:0] cmd_next;
    // Address
    reg [31:0] addr;
    logic [31:0] addr_next;

    // Internal clock to deal with clock polarity
    logic clock;
    assign clock = CPOL ^ sck;

    // Quad data stuff
    logic quad_io, quad_addr_next, quad_data_next;

    assign quad_addr_next = (cmd_next == CMD_QUAD_IO_FAST_READ) || (cmd_next == CMD_QUAD_IO_FAST_READ4);
    assign quad_data_next = quad_addr_next || (cmd_next == CMD_QUAD_O_FAST_READ) || (cmd_next == CMD_QUAD_O_FAST_READ4);
    assign quad_io = (state == GET_ADDRQ) || (state == PUT_DATAQ);

    // State machine stuff
    logic addr_4byte, cmd_done, addr_done, dummy_done, cmd_has_dummy, cmd_valid_next, data_count_aligned, incr_addr;

    assign addr_4byte = (cmd == CMD_READ4) || (cmd == CMD_FAST_READ4) || (cmd == CMD_QUAD_O_FAST_READ4) || (cmd == CMD_QUAD_IO_FAST_READ4);
    assign cmd_valid_next =
        (cmd_next == CMD_READ)  || (cmd_next == CMD_FAST_READ)  || (cmd_next == CMD_QUAD_O_FAST_READ) || (cmd_next == CMD_QUAD_IO_FAST_READ) ||
        (cmd_next == CMD_READ4) || (cmd_next == CMD_FAST_READ4) || (cmd_next == CMD_QUAD_O_FAST_READ4) || (cmd_next == CMD_QUAD_IO_FAST_READ4);
    assign cmd_done = (state == GET_CMD) && (data_count_next == 6'd8);
    assign addr_done = ((state == GET_ADDRS) || (state == GET_ADDRQ)) && (data_count_next == (addr_4byte ? 6'd40 : 6'd32));
    assign dummy_done = dummy == (DUMMY_CYCLES - 1);
    assign cmd_has_dummy = (cmd != CMD_READ) && (cmd != CMD_READ4);
    assign data_count_aligned = (data_count_next[2:0] == 3'd0);
    assign incr_addr = ((state == PUT_DATAS) || (state == PUT_DATAQ)) && data_count_aligned; // We let the counter free run...this just increments every byte

    assign data_buf_next = quad_io ? {data_buf[27:0], dq_in} : {data_buf[30:0], dq_in[0]};
    assign data_count_next = (state == DUMMY) ? data_count : data_count + (quad_io ? 6'd4 : 6'd1) ;
    assign dummy_next = (state == DUMMY) ? dummy + 8'd1 : 8'd0 ;

    always_comb begin
        if (state == DUMMY) addr_next = addr;
        else if (addr_done) addr_next = addr_4byte ? data_buf_next : (data_buf_next & 32'h00ff_ffff);
        else if (incr_addr) addr_next = addr + 32'd1;
        else addr_next = addr;
    end

    assign cmd_next = cmd_done ? data_buf_next[7:0] : cmd ;

    // Output driving
    reg [7:0] data_out;
    logic [7:0] data_out_next;
    reg dq_drive_reg;

    assign dq_out[3] = data_out[7];
    assign dq_out[2] = data_out[6];
    assign dq_out[1] = (quad_io ? data_out[5] : data_out[7]);
    assign dq_out[0] = data_out[4];

    assign dq_drive[3] = (dq_drive_reg && quad_io);
    assign dq_drive[2] = (dq_drive_reg && quad_io);
    assign dq_drive[1] =  dq_drive_reg;
    assign dq_drive[0] = (dq_drive_reg && quad_io);

    assign mem_req_addr = addr_next[ADDR_BITS-1:0];
    assign mem_req_data = 8'd0; // write is unimplemented
    assign mem_req_r_wb = 1'b1; // write is unimplemented
    assign mem_req_valid = ((state_next == PUT_DATAS) || (state_next == PUT_DATAQ)) && data_count_aligned;

    always_comb begin
        if (data_count[2:0] == 3'd0)
            data_out_next = mem_resp_data;
        else if (quad_io)
            data_out_next = {data_out[3:0], 4'd0};
        else
            data_out_next = {data_out[6:0], 1'd0};
    end

    // Not using always_ff to get around a verilator issue
    always @(posedge clock or negedge clock or posedge reset or posedge cs) begin
        if (reset | cs) begin
            data_count <= 6'd0;
            state <= STANDBY;
            dummy <= 8'd0;
            dq_drive_reg <= 1'b0;
            cmd <= 8'd0;
        end else begin
            if (clock ^ CPHA) begin
                // Capture edge
                data_buf    <= data_buf_next;
                data_count  <= data_count_next;
                state       <= state_next;
                dummy       <= dummy_next;
                addr        <= addr_next;
                cmd         <= cmd_next;
            end else begin
                // Launch edge
                dq_drive_reg <= ((state == PUT_DATAS) || (state == PUT_DATAQ));
                data_out <= data_out_next;
            end
        end
    end

    always_comb begin
        state_next = state;
        casez(state)
            STANDBY:
                if (!cs) state_next = GET_CMD;
            GET_CMD:
                if (cmd_done) begin
                    if (cmd_valid_next)
                        state_next = (quad_addr_next ? GET_ADDRQ : GET_ADDRS);
                    else
                        state_next = ERROR;
                end
            GET_ADDRS:
                if (addr_done) begin
                    if (cmd_has_dummy)
                        state_next = DUMMY;
                    else if (quad_data_next)
                        state_next = PUT_DATAQ;
                    else
                        state_next = PUT_DATAS;
                end
            GET_ADDRQ:
                if (addr_done) begin
                    if (cmd_has_dummy)
                        state_next = DUMMY;
                    else if (quad_data_next)
                        state_next = PUT_DATAQ;
                    else
                        state_next = PUT_DATAS;
                end
            DUMMY:
                if (dummy_done) begin
                    if (quad_data_next)
                        state_next = PUT_DATAQ;
                    else
                        state_next = PUT_DATAS;
                end
            PUT_DATAS:
                if (cs) state_next = STANDBY;
            PUT_DATAQ:
                if (cs) state_next = STANDBY;
            ERROR:
                if (cs) state_next = STANDBY;
        endcase
    end

endmodule
