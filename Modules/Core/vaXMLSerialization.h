#pragma once

#include "vaCore.h"

#include "IntegratedExternals\vaTinyxml2Integration.h"

namespace VertexAsylum
{
    class vaXMLSerializer;

    class vaXMLSerializable
    {
    protected:
        vaXMLSerializable( ) { };
        virtual ~vaXMLSerializable( ) { };

    protected:
        friend class vaXMLSerializer;

        // By informal convention, SerializeOpenChildElement / SerializePopToParentElement are done by the caller before & after calling vaXMLSerializable::Serialize
        // For scenarios where an object names itself (does open/pop element itself), it might be best not to use vaXMLSerializable but just a simple public NamedSerialize() 
        virtual bool                                Serialize( vaXMLSerializer & serializer ) = 0;
    };

    class vaXMLSerializer
    {
    private:
        tinyxml2::XMLPrinter        m_writePrinter;
        vector<string>              m_writeElementNameStack;
        vector<map<string, bool>>   m_writeElementNamesMapStack;
        int                         m_writeElementStackDepth    = 0;
        bool                        m_writeElementPrevWasOpen   = false;    // used to figure out if we just wrote a leaf or another element(s)

        tinyxml2::XMLDocument       m_readDocument;
        tinyxml2::XMLElement *      m_currentReadElement        = nullptr;

        bool                        m_isReading                 = false;
        bool                        m_isWriting                 = false;

        // version history:
        // -1 - no version tracking, all saved as attributes
        //  0 - added version tracking, not using attributes anymore (intended way of using XML)
        //  1 - vaXMLSerializable::Serialize should no longer be required to open their own sub-elements (although it should still be free to do it) - this totally breaks backward compatibility
        int                         m_formatVersion             = -1;

    public:
        // parse data, set to loading mode
        vaXMLSerializer( const char * inputData, size_t dataSize )
        {
            InitReadingFromBuffer( inputData, dataSize );
        }

        // parse data, set to loading mode
        explicit vaXMLSerializer( vaFileStream & fileStream )
        {
            int64 fileLength = fileStream.GetLength( );
            if( fileLength == 0 )
                { assert( false ); return; }

            if( fileStream.GetPosition() != 0 )
                fileStream.Seek( 0 );

            char * buffer = new char[ fileLength+1 ];
            if( fileStream.Read( buffer, fileLength ) )
            {
                buffer[fileLength] = 0; // null-terminate
                InitReadingFromBuffer( buffer, fileLength+1 );
            }
            else { assert( false ); } // error reading?
            delete[] buffer;
        }

        // open printer, set to storing mode
        vaXMLSerializer( ) : m_writePrinter( nullptr, false, 0 )
        {
            m_isWriting = true;
            m_isReading = false;
            m_currentReadElement = nullptr;

            m_writeElementNamesMapStack.push_back( map<string, bool>{ } );

            // version info
            if( WriterOpenElement( "vaXMLSerializer" ) )
            {
                m_formatVersion = 1;
                m_writePrinter.PushText( m_formatVersion ); 
                WriterCloseElement( "vaXMLSerializer", true );
            }
        }

        ~vaXMLSerializer( )
        {
            assert( m_currentReadElement == nullptr );      // forgot to ReadPopToParentElement?
            assert( m_writeElementNameStack.size() == 0 );  // forgot to WriteCloseElement?
            if( IsWriting() )
                { assert( m_writeElementNamesMapStack.size() == 1 ); }
            else
                { assert( m_writeElementNamesMapStack.size() == 0 ); }

            assert( m_writeElementStackDepth == 0 );        // forgot to WriteCloseElement?
        }

        void InitReadingFromBuffer( const char * inputData, size_t dataSize )
        {
            assert( m_isReading == false );
            assert( m_isWriting == false );
            assert( m_currentReadElement == nullptr );
            m_isReading = m_readDocument.Parse( inputData, dataSize ) == tinyxml2::XMLError::XML_SUCCESS;
            assert( m_isReading ); // error parsing?

            // version info
            if( ReaderAdvanceToChildElement( "vaXMLSerializer" ) )
            {
                if( m_currentReadElement->QueryIntText( &m_formatVersion ) != tinyxml2::XML_SUCCESS )
                { assert( false ); }        // can't read version?
                if( m_formatVersion < 0 || m_formatVersion > 1 )
                { assert( false ); }        // unsupported version
                ReaderPopToParentElement( "vaXMLSerializer" );
            }
        }

        bool                        IsReading( ) const { return m_isReading; }
        bool                        IsWriting( ) const { return m_isWriting; }

        tinyxml2::XMLPrinter &      GetWritePrinter( ) { assert( m_isWriting ); return m_writePrinter; }

        tinyxml2::XMLDocument &     GetReadDocument( ) { assert( m_isReading ); return m_readDocument; }

        tinyxml2::XMLElement *      GetCurrentReadElement( );

    private:

        bool                        ReaderAdvanceToChildElement( const char * name );
        bool                        ReaderAdvanceToSiblingElement( const char * name );
        bool                        ReaderPopToParentElement( const char * nameToVerify );

        int                         ReaderCountChildren( const char * elementName, const char * childName = nullptr );

        bool                        ReadBoolAttribute( const char * name, bool & outVal )     const;
        bool                        ReadInt32Attribute( const char * name, int32 & outVal )   const;
        bool                        ReadUInt32Attribute( const char * name, uint32 & outVal ) const;
        bool                        ReadInt64Attribute( const char * name, int64 & outVal )   const;
        bool                        ReadFloatAttribute( const char * name, float & outVal )   const;
        bool                        ReadDoubleAttribute( const char * name, double & outVal ) const;
        bool                        ReadStringAttribute( const char * name, string & outVal ) const;

        bool                        ReadAttribute( const char * name, bool & outVal )       const { return ReadBoolAttribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, int32 & outVal )      const { return ReadInt32Attribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, uint32 & outVal )     const { return ReadUInt32Attribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, int64 & outVal )      const { return ReadInt64Attribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, float & outVal )      const { return ReadFloatAttribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, double & outVal )     const { return ReadDoubleAttribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, string & outVal )     const { return ReadStringAttribute( name, outVal ); }
        bool                        ReadAttribute( const char * name, vaGUID & val )        const;
        bool                        ReadAttribute( const char * name, vaVector3 & val )     const;
        bool                        ReadAttribute( const char * name, vaVector4 & val )     const;

    private:

        bool                        WriterOpenElement( const char * name, bool mustBeUnique = true );
        bool                        WriterCloseElement( const char * nameToVerify, bool compactMode = false );

        bool                        WriteAttribute( const char * name, bool  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, int32  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, uint32  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, int64  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, float  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, double  outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal ); return true; }
        bool                        WriteAttribute( const char * name, const string & outVal ) { assert( m_isWriting ); if( !m_isWriting ) return false; m_writePrinter.PushAttribute( name, outVal.c_str( ) ); return true; }
        bool                        WriteAttribute( const char * name, vaGUID & val );
        bool                        WriteAttribute( const char * name, vaVector3 & val );
        bool                        WriteAttribute( const char * name, vaVector4 & val );

    private:
        bool                        SerializeInternal( bool & val )                 { if( m_isWriting ) { m_writePrinter.PushText( val ); return true; };                                   if( m_currentReadElement == nullptr || !m_isReading ) return false; return m_currentReadElement->QueryBoolText( &val )  == tinyxml2::XML_SUCCESS; }
        bool                        SerializeInternal( int32 & val )                { if( m_isWriting ) { m_writePrinter.PushText( val ); return true; };                                   if( m_currentReadElement == nullptr || !m_isReading ) return false; return m_currentReadElement->QueryIntText( &val )  == tinyxml2::XML_SUCCESS; }
        bool                        SerializeInternal( uint32 & val )               { if( m_isWriting ) { m_writePrinter.PushText( val ); return true; };                                   if( m_currentReadElement == nullptr || !m_isReading ) return false; return m_currentReadElement->QueryUnsignedText( &val )  == tinyxml2::XML_SUCCESS; }
        bool                        SerializeInternal( int64 & val )                { if( m_isWriting ) { m_writePrinter.PushText( val ); return true; };                                   if( m_currentReadElement == nullptr || !m_isReading ) return false; return m_currentReadElement->QueryInt64Text( &val )  == tinyxml2::XML_SUCCESS; }
        bool                        SerializeInternal( float & val )                { if( m_isWriting ) { m_writePrinter.PushText( val ); return true; };                                   if( m_currentReadElement == nullptr || !m_isReading ) return false; return m_currentReadElement->QueryFloatText( &val )  == tinyxml2::XML_SUCCESS; }
        bool                        SerializeInternal( double & val )               { if( m_isWriting ) { m_writePrinter.PushText( val ); return true; };                                   if( m_currentReadElement == nullptr || !m_isReading ) return false; return m_currentReadElement->QueryDoubleText( &val )  == tinyxml2::XML_SUCCESS; }
        bool                        SerializeInternal( string & val )               { if( m_isWriting ) { m_writePrinter.PushText( val.c_str(), false ); return true; };                    if( m_currentReadElement == nullptr || !m_isReading || m_currentReadElement->GetText( ) == nullptr ) return false; val = m_currentReadElement->GetText( ); return true; }
        bool                        SerializeInternal( pair<string, string> & val ) { return SerializeInternal( "first", val.first ) && SerializeInternal( "second", val.second ); }
        bool                        SerializeInternal( pair<string, bool> & val )   { return SerializeInternal( "first", val.first ) && SerializeInternal( "second", val.second ); }
        bool                        SerializeInternal( vaGUID & val )               { if( m_isWriting ) { m_writePrinter.PushText( vaCore::GUIDToStringA( val ).c_str() ); return true; };  if( m_currentReadElement == nullptr || !m_isReading || m_currentReadElement->GetText( ) == nullptr ) return false; val = vaCore::GUIDFromStringA( m_currentReadElement->GetText() ); return true; }
        bool                        SerializeInternal( vaVector3 & val )            { if( m_isWriting ) { m_writePrinter.PushText( vaVector3::ToString( val ).c_str() ); return true; };    if( m_currentReadElement == nullptr || !m_isReading || m_currentReadElement->GetText( ) == nullptr ) return false; return vaVector3::FromString( m_currentReadElement->GetText(), val ); }
        bool                        SerializeInternal( vaVector4 & val )            { if( m_isWriting ) { m_writePrinter.PushText( vaVector4::ToString( val ).c_str() ); return true; };    if( m_currentReadElement == nullptr || !m_isReading || m_currentReadElement->GetText( ) == nullptr ) return false; return vaVector4::FromString( m_currentReadElement->GetText(), val ); }
        bool                        SerializeInternal( vaXMLSerializable & val )    { return val.Serialize( *this ); }

        bool                        SerializeInternal( const char * name, bool & val )                  { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, int32 & val )                 { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, uint32 & val )                { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, int64 & val )                 { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, float & val )                 { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, double & val )                { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, string & val )                { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, vaGUID & val )                { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, vaVector3 & val )             { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, vaVector4 & val )             { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, vaXMLSerializable & val )     { if( m_formatVersion < 0 ) return OldSerializeValue( name, val ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, pair<string, string> & val )  { if( m_formatVersion < 0 ) { assert( false ); return false; } if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
        bool                        SerializeInternal( const char * name, pair<string, bool> & val )    { if( m_formatVersion < 0 ) { assert( false ); return false; } if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }

    public:

        int                         GetVersion( ) const                                         { return m_formatVersion; }

        bool                        WriterSaveToFile( vaFileStream & fileStream );

        // Serialization helpers - both reader & writer
        bool                        SerializeOpenChildElement( const char * name );
        bool                        SerializePopToParentElement( const char * nameToVerify );

        //////////////////////////////////////////////////////////////////////////
        // OLD INTERFACE
    private:
        // Simple ("value") types use attributes for serialization because they take up less space
        bool                        OldSerializeValue( const char * name, bool & val )         { assert( m_formatVersion < 0 ); if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        OldSerializeValue( const char * name, int32 & val )        { assert( m_formatVersion < 0 ); if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        OldSerializeValue( const char * name, uint32 & val )       { assert( m_formatVersion < 0 ); if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        OldSerializeValue( const char * name, int64 & val )        { assert( m_formatVersion < 0 ); if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        OldSerializeValue( const char * name, float & val )        { assert( m_formatVersion < 0 ); if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        OldSerializeValue( const char * name, double & val )       { assert( m_formatVersion < 0 ); if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        OldSerializeValue( const char * name, string & val )       { assert( m_formatVersion < 0 ); if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        OldSerializeValue( const char * name, vaGUID & val )       { assert( m_formatVersion < 0 ); if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        OldSerializeValue( const char * name, vaVector3 & val )    { assert( m_formatVersion < 0 ); if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        OldSerializeValue( const char * name, vaVector4 & val )    { assert( m_formatVersion < 0 ); if( m_isWriting ) return WriteAttribute( name, val ); if( m_isReading ) return ReadAttribute( name, val ); assert( false ); return false; }
        bool                        OldSerializeValue( const char * name, vaXMLSerializable & val )    { assert( m_formatVersion < 0 ); if( !SerializeOpenChildElement( name ) ) return false; bool retVal = SerializeInternal( val ); if( !SerializePopToParentElement( name ) ) { assert( false ); return false; }; return retVal; }
    public:
        //
        // A version of OldSerializeValue that will simply, if reading, set default if value is missing and always returns true
        template< typename ValueType >
        bool                        OldSerializeValue( const char * name, ValueType & val, const ValueType & defaultVal );
        //
        // Generic version with no default value (returns false if no value read)
        template< typename ValueType >
        bool                        OldSerializeValue( const char * name, ValueType & val )                                 { return OldSerializeValue( name, (ValueType &)val ); }
        //
        // support for vaXMLSerializable objects
        template< typename ElementType >
        bool                        OldSerializeObjectVector( const char * name, vector< shared_ptr<ElementType> > & objects );
        // support for vaXMLSerializable objects
        template< typename ElementType >
        bool                        OldSerializeObjectVector( const char * name, vector< ElementType > & objects );
        //
        // support for native value types (bool, int32, float, string, vaGUID, etc., see above)
        template< typename ElementType >
        bool                        OldSerializeValueVector( const char * name, vector< ElementType > & elements );
        //
        // OLD INTERFACE
        //////////////////////////////////////////////////////////////////////////

        // A version of Serialize that will simply, if reading, set default if value is missing and always returns true
        template< typename ValueType >
        bool                        Serialize( const char * name, ValueType & val, const ValueType & defaultVal );
        //
        // Generic version with no default value (returns false if no value read)
        template< typename ValueType >
        bool                        Serialize( const char * name, ValueType & val )                                         { return SerializeInternal( name, val ); }

        // string-based versions
        template< typename ValueType >
        bool                        Serialize( const string & name, ValueType & val, const ValueType & defaultVal )         { return Serialize<ValueType>( name.c_str(), val, defaultVal ); }
        template< typename ValueType >
        bool                        Serialize( const string & name, ValueType & val )                                       { return Serialize<ValueType>( name.c_str(), val ); }

        // generic array read - user-provided setupCallback will either receive number of items in the array (when isReading == true) or needs to return the 
        // count itself (when isReading == false), while itemCallback provides handling of per-item serialization 
        template< typename ContainerType  >
        bool                        SerializeArrayGeneric( const char * containerName, const char * itemName, ContainerType & container, std::function< void( bool isReading, ContainerType & container, int & itemCount ) > setupCallback, std::function< void( vaXMLSerializer & serializer, ContainerType & container, int index ) > itemCallback );

        template< typename ItemType >
        bool                        SerializeArray( const char * containerName, const char * itemName, vector< ItemType > & elements );

        template< typename ItemType >
        bool                        SerializeArray( const char * containerName, const char * itemName, vector< shared_ptr<ItemType> > & elements );
    };


    inline tinyxml2::XMLElement * vaXMLSerializer::GetCurrentReadElement( ) { return m_currentReadElement; }
    inline bool                   vaXMLSerializer::ReaderAdvanceToChildElement( const char * name )
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        tinyxml2::XMLElement * e = ( m_currentReadElement != nullptr ) ? ( m_currentReadElement->FirstChildElement( name ) ) : ( m_readDocument.FirstChildElement( name ) );
        if( e != nullptr ) { m_currentReadElement = e; return true; } return false;
    }
    inline bool                   vaXMLSerializer::ReaderAdvanceToSiblingElement( const char * name )
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        tinyxml2::XMLElement * e = ( m_currentReadElement != nullptr ) ? ( m_currentReadElement->NextSiblingElement( name ) ) : ( m_readDocument.NextSiblingElement( name ) );
        if( e != nullptr )
        {
            m_currentReadElement = e; return true;
        }
        return false;
    }
    inline bool                   vaXMLSerializer::ReaderPopToParentElement( const char * nameToVerify )
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( nameToVerify != nullptr )
        {
            assert( m_currentReadElement != nullptr && strncmp( m_currentReadElement->Name(), nameToVerify, 32768 ) == 0 );
        }

        if( m_currentReadElement == nullptr )
            return false;
        tinyxml2::XMLElement * e = ( m_currentReadElement->Parent( ) != nullptr ) ? ( m_currentReadElement->Parent( )->ToElement( ) ) : ( nullptr );
        m_currentReadElement = e;
        return true;
    }
    inline bool                   vaXMLSerializer::ReadBoolAttribute( const char * name, bool & outVal )   const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryBoolAttribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadInt32Attribute( const char * name, int32 & outVal )   const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryIntAttribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadUInt32Attribute( const char * name, uint32 & outVal ) const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryUnsignedAttribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadInt64Attribute( const char * name, int64 & outVal )   const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryInt64Attribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadFloatAttribute( const char * name, float & outVal )   const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryFloatAttribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadDoubleAttribute( const char * name, double & outVal ) const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        return m_currentReadElement->QueryDoubleAttribute( name, &outVal ) == tinyxml2::XML_SUCCESS;
    }
    inline bool                   vaXMLSerializer::ReadStringAttribute( const char * name, string & outVal ) const
    {
        assert( m_isReading ); if( !m_isReading ) return false;
        if( m_currentReadElement == nullptr )   return false;
        const tinyxml2::XMLAttribute * a = ( ( const tinyxml2::XMLElement * )m_currentReadElement )->FindAttribute( name );
        if( a != nullptr )
        {
            outVal = a->Value( );
            return true;
        }
        return false;
    }

    inline bool                 vaXMLSerializer::WriterOpenElement( const char * name, bool mustBeUnique )
    {
        assert( m_isWriting ); if( !m_isWriting ) return false;

        if( mustBeUnique && m_writeElementNamesMapStack.back( ).find( string( name ) ) != m_writeElementNamesMapStack.back( ).end( ) )
        {
            assert( false ); // element with the same name already exists - this is not permitted except for arrays
            return false;
        }

        m_writePrinter.OpenElement( name );
        m_writeElementStackDepth++;
        m_writeElementNameStack.push_back( name );
        m_writeElementPrevWasOpen = true;

        m_writeElementNamesMapStack.push_back( map<string, bool>{ } );  // add empty map for child element's duplicate name tracking
        return true;
    }

    inline bool                 vaXMLSerializer::WriterCloseElement( const char * nameToVerify, bool compactMode )
    {
        if( nameToVerify != nullptr )
        {
            assert( m_writeElementNameStack.size() > 0 && m_writeElementNameStack.back() == nameToVerify );
        }
        m_writeElementNamesMapStack.pop_back( ); // go back to parent element map of names
        m_writeElementNamesMapStack.back().insert( std::pair<string, bool>( m_writeElementNameStack.back(), true ) ); // add this one to the list

        assert( m_isWriting ); if( !m_isWriting ) return false;
        assert( m_writeElementStackDepth > 0 ); if( m_writeElementStackDepth <= 0 ) return false;
        m_writePrinter.CloseElement( compactMode );
        m_writeElementNameStack.pop_back();
        m_writeElementStackDepth--;
        m_writeElementPrevWasOpen = false;

        return true;
    }

    inline bool                 vaXMLSerializer::WriterSaveToFile( vaFileStream & fileStream )
    {
        assert( m_isWriting ); if( !m_isWriting ) return false;
        fileStream.Seek( 0 );
        fileStream.Truncate( );
        fileStream.Write( GetWritePrinter( ).CStr( ), GetWritePrinter( ).CStrSize( ) - 1 );
        fileStream.Flush( );
        return true;
    }

    inline bool                 vaXMLSerializer::ReadAttribute( const char * name, vaGUID & val ) const
    {
        string uiid;
        if( !ReadStringAttribute( name, uiid ) )
            { assert(false); return false; }

        val = vaCore::GUIDFromStringA( uiid );
        return true;
    }

    inline bool                 vaXMLSerializer::WriteAttribute( const char * name, vaGUID & val )
    {
        return WriteAttribute( name, vaCore::GUIDToStringA( val ) );
    }

    inline bool                 vaXMLSerializer::ReadAttribute( const char * name, vaVector3 & val )     const
    {
        string valStr;
        if( !ReadStringAttribute( name, valStr ) )
            { assert(false); return false; }

        return vaVector3::FromString( valStr, val );
    }
    inline bool                 vaXMLSerializer::WriteAttribute( const char * name, vaVector3 & val )
    {
        return WriteAttribute( name, vaVector3::ToString( val ) );
    }

    inline bool                 vaXMLSerializer::ReadAttribute( const char * name, vaVector4 & val )     const
    {
        string valStr;
        if( !ReadStringAttribute( name, valStr ) )
            { assert(false); return false; }

        return vaVector4::FromString( valStr, val );
    }
    inline bool                 vaXMLSerializer::WriteAttribute( const char * name, vaVector4 & val )
    {
        return WriteAttribute( name, vaVector4::ToString( val ) );
    }

    inline bool                 vaXMLSerializer::SerializeOpenChildElement( const char * name ) 
    { 
        if( m_isWriting ) 
            return WriterOpenElement( name );   
        if( m_isReading ) 
            return ReaderAdvanceToChildElement( name );
        assert( false ); 
        return false; 
    }

    template< typename ValueType >
    inline bool                 vaXMLSerializer::OldSerializeValue( const char * name, ValueType & val, const ValueType & defaultVal ) 
    { 
        assert( m_formatVersion < 0 );
        if( m_isWriting ) return OldSerializeValue<ValueType>( name, val ); 
        if( m_isReading ) 
        {
            bool ret = ReadAttribute( name, val );
            if( !ret )
                val = defaultVal;
            return true;    // always return true
        }
        assert( false );
        return false;
    }

    template< typename ValueType >
    inline bool                 vaXMLSerializer::Serialize( const char * name, ValueType & val, const ValueType & defaultVal ) 
    { 
        if( m_formatVersion < 0 ) return OldSerializeValue<ValueType>( name, val, defaultVal );
        if( m_isWriting ) return SerializeInternal( name, val ); 
        if( m_isReading ) 
        {
            bool ret = false;
            if( SerializeOpenChildElement( name ) )
            {
                ret = SerializeInternal( val ); 
                if( !SerializePopToParentElement( name ) ) 
                { assert( false ); return false; }; 
            }
            if( !ret )
                val = defaultVal;
            return true;    // always return true
        }
        assert( false );
        return false;
    }

    inline bool                 vaXMLSerializer::SerializePopToParentElement( const char * nameToVerify ) 
    { 
        if( m_isWriting ) 
        {
            return WriterCloseElement( nameToVerify );
        }
        if( m_isReading ) 
        {   
            return ReaderPopToParentElement( nameToVerify ); 
        }
        assert( false ); 
        return false; 
    }

    template< typename ElementType >
    inline bool                 vaXMLSerializer::OldSerializeObjectVector( const char * name, vector< shared_ptr<ElementType> > & objects )
    {
        assert( m_formatVersion < 0 );

        if( !SerializeOpenChildElement( name ) )
            { assert(false); return false; }

        uint32 count = (uint32)objects.size();
        if( !OldSerializeValue<uint32>( "count", count ) )
            { assert(false); return false; }
        if( m_isReading )
            objects.resize( count );
        string elementName;
        for( uint32 i = 0; i < count; i++ )
        {
            elementName = vaStringTools::Format( "a%d", i );
            if( !SerializeOpenChildElement( elementName.c_str() ) )
                { assert(false); return false; }
            if( m_isReading )
                objects[i] = std::make_shared<ElementType>( );
            SerializeInternal( (ElementType&)*objects[i] );
            if( !SerializePopToParentElement( elementName.c_str() ) )
                { assert(false); return false; }
        }

        if( !SerializePopToParentElement( name ) )
            { assert(false); return false; }

        return true;
    }

    template< typename ElementType >
    inline bool                 vaXMLSerializer::OldSerializeObjectVector( const char * name, vector< ElementType > & objects )
    {
        assert( m_formatVersion < 0 );

        if( !SerializeOpenChildElement( name ) )
            { assert(false); return false; }

        uint32 count = (uint32)objects.size();
        if( !OldSerializeValue<uint32>( "count", count ) )
            { assert(false); return false; }
        if( m_isReading )
            objects.resize( count );
        string elementName;
        for( uint32 i = 0; i < count; i++ )
        {
            elementName = vaStringTools::Format( "a%d", i );
            if( !SerializeOpenChildElement( elementName.c_str() ) )
                { assert(false); return false; }
            objects[i].Serialize( *this );
            if( !SerializePopToParentElement( elementName.c_str() ) )
                { assert(false); return false; }
        }

        if( !SerializePopToParentElement( name ) )
            { assert(false); return false; }

        return true;
    }

    template< typename ElementType >
    inline bool                 vaXMLSerializer::OldSerializeValueVector( const char * name, vector< ElementType > & elements )
    {
        assert( m_formatVersion < 0 );

        if( !SerializeOpenChildElement( name ) )
            { assert(false); return false; }

        uint32 count = (uint32)elements.size();
        if( !OldSerializeValue<uint32>( "count", count ) )
            { assert(false); return false; }
        if( m_isReading )
            elements.resize( count );
        for( uint32 i = 0; i < count; i++ )
        {
            if( !OldSerializeValue<ElementType>( vaStringTools::Format( "a%d", i ).c_str(), elements[i] ) )
                { assert(false); return false; }
        }

        if( !SerializePopToParentElement( name ) )
            { assert(false); return false; }

        return true;
    }

    inline int vaXMLSerializer::ReaderCountChildren( const char * elementName, const char * childName )
    {
        // assert( m_formatVersion >= 0 );
        if( !m_isReading )
        {
            assert( false );
            return -1;
        }

        if( !ReaderAdvanceToChildElement( elementName ) ) 
            return -1;

        int counter = 0;
        if( ReaderAdvanceToChildElement( childName ) )
        {
            do
            {
                counter++;
            } while( ReaderAdvanceToSiblingElement( childName ) );
            ReaderPopToParentElement( childName );
        }

        SerializePopToParentElement( elementName );
        return counter;
    }

    template< typename ContainerType >
    inline bool vaXMLSerializer::SerializeArrayGeneric(  const char * containerName, const char * itemName, ContainerType & container, std::function< void( bool isReading, ContainerType & container, int & itemCount ) > setupCallback, std::function< void( vaXMLSerializer & serializer, ContainerType & container, int index ) > itemCallback )
    {
        // assert( m_formatVersion >= 0 );
        assert( containerName != nullptr );
        assert( itemName != nullptr );
        int itemCount = 0;

        if( IsReading() )
        {
            itemCount = ReaderCountChildren( containerName, itemName );
            if( itemCount < 0 )
                return false;
            int itemCountCopy = itemCount;
            setupCallback( true, container, itemCountCopy );
            assert( itemCountCopy == itemCount ); // it is invalid to change itemCount in the callback during isReading
        }
        else
        {
            setupCallback( false, container, itemCount );
            if( itemCount < 0 )
                return false;
        }

        if( !SerializeOpenChildElement( containerName ) )
            return false;

        if( m_isReading ) 
        { 
            // just an additional sanity check
            if( m_formatVersion >= 0 )
            {   
                bool isArray = false;
                if( !ReadAttribute( "array", isArray ) || !isArray )
                    { assert( false ); }
            }

            int counter = 0;
            if( ReaderAdvanceToChildElement( itemName ) )
            {
                do
                {
                    itemCallback( *this, container, counter );
                    counter++;
                } while( ReaderAdvanceToSiblingElement( itemName ) && counter < itemCount );
                ReaderPopToParentElement( itemName );
            }
            assert( counter == itemCount ); // must be the same as returned by ReaderCountChildren
        }
        else if( m_isWriting )
        {
            // just an additional sanity check
            if( m_formatVersion >= 0 )
                WriteAttribute( "array", true );

            for( int i = 0; i < itemCount; i++ )
            {
                WriterOpenElement( itemName, false );
                itemCallback( *this, container, i );
                WriterCloseElement( itemName, m_writeElementPrevWasOpen );  // if there were no sub-elements (itemName element contains only data) then use compact mode!
            }
        }
        else { assert( false ); }

        bool allOk = SerializePopToParentElement( containerName );
        assert( allOk );
        return allOk;
    }

    template< typename ItemType >
    inline bool vaXMLSerializer::SerializeArray( const char * containerName, const char * itemName, vector< ItemType > & elements )  
    { 
        assert( m_formatVersion >= 0 );
        return SerializeArrayGeneric<vector<ItemType>>( containerName, itemName, elements, 
            [ ] ( bool isReading, vector<ItemType> & container, int & itemCount )
            { 
                if( isReading )
                    container.resize( itemCount );
                else
                    itemCount = (int)container.size();
            }, 
            [ ] ( vaXMLSerializer & serializer, vector<ItemType> & container, int index ) 
            { 
                serializer.SerializeInternal( container[index] ); 
            } 
            ); 
    }

    template< typename ItemType >
    inline bool vaXMLSerializer::SerializeArray( const char * containerName, const char * itemName, vector< shared_ptr<ItemType> > & elements )  
    {
        assert( m_formatVersion >= 0 );
        return SerializeArrayGeneric<vector<shared_ptr<ItemType>>>( containerName, itemName, elements, 
            [ ] ( bool isReading, vector<shared_ptr<ItemType>> & container, int & itemCount )
            { 
                if( isReading )
                {
                    container.resize( itemCount );
                    for( auto & item : container )
                        item = std::make_shared<ItemType>( );
                }
                else
                    itemCount = (int)container.size();
            }, 
            [ ] ( vaXMLSerializer & serializer, vector<shared_ptr<ItemType>> & container, int index ) 
            { 
                serializer.SerializeInternal( *std::static_pointer_cast<vaXMLSerializable, ItemType>( container[index] ) ); 
            } 
            ); 
    }

}