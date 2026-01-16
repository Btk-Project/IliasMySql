-- PostgreSQL Test Database Initialization Script
-- This script creates test tables with all supported data types

-- Test table for type conversion testing
CREATE TABLE test_types (
    id SERIAL PRIMARY KEY,
    bool_col BOOLEAN,
    int2_col SMALLINT,
    int4_col INTEGER,
    int8_col BIGINT,
    float4_col REAL,
    float8_col DOUBLE PRECISION,
    numeric_col NUMERIC(10, 2),
    text_col TEXT,
    varchar_col VARCHAR(255),
    bytea_col BYTEA,
    date_col DATE,
    time_col TIME,
    timestamp_col TIMESTAMP,
    timestamptz_col TIMESTAMPTZ,
    nullable_col TEXT
);

-- Insert sample data for testing
INSERT INTO test_types (
    bool_col, int2_col, int4_col, int8_col,
    float4_col, float8_col, numeric_col,
    text_col, varchar_col, bytea_col,
    date_col, time_col, timestamp_col, timestamptz_col,
    nullable_col
) VALUES
    (true, 32767, 2147483647, 9223372036854775807,
     3.14, 3.141592653589793, 12345.67,
     'Hello, World!', 'Test varchar', E'\\xDEADBEEF',
     '2024-01-15', '14:30:00', '2024-01-15 14:30:00', '2024-01-15 14:30:00+00',
     'not null'),
    (false, -32768, -2147483648, -9223372036854775808,
     -3.14, -3.141592653589793, -12345.67,
     'Another text', 'Another varchar', E'\\x00010203',
     '2000-12-31', '23:59:59', '2000-12-31 23:59:59', '2000-12-31 23:59:59+00',
     NULL);

-- Test table for streaming result set testing (larger dataset)
CREATE TABLE test_streaming (
    id SERIAL PRIMARY KEY,
    value INTEGER,
    description TEXT
);

-- Insert multiple rows for streaming tests
INSERT INTO test_streaming (value, description)
SELECT 
    generate_series(1, 1000) as value,
    'Row ' || generate_series(1, 1000) as description;

-- Test table for prepared statement testing
CREATE TABLE test_prepared (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    age INTEGER,
    active BOOLEAN,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Test table for transaction testing
CREATE TABLE test_transactions (
    id SERIAL PRIMARY KEY,
    balance NUMERIC(10, 2) NOT NULL DEFAULT 0,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert initial data for transaction tests
INSERT INTO test_transactions (balance) VALUES (100.00), (200.00), (300.00);
